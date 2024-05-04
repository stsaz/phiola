/** phiola: modify file tags in-place
2022, Simon Zolin */

#include <phiola.h>
#include <format/mmtag.h>
#include <avpack/id3v1.h>
#include <avpack/id3v2.h>
#include <ffsys/file.h>
#include <ffsys/path.h>

#define syserrlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR | PHI_LOG_SYS, "tag", NULL, __VA_ARGS__)
#define errlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR, "tag", NULL, __VA_ARGS__)
#define warnlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_WARN, "tag", NULL, __VA_ARGS__)
#define infolog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_INFO, "tag", NULL, __VA_ARGS__)
#define dbglog(...) \
do { \
	if (core->conf.log_level >= PHI_LOG_DEBUG) \
		core->conf.log(core->conf.log_obj, PHI_LOG_DEBUG, "tag", NULL, __VA_ARGS__); \
} while (0)

extern const phi_core *core;
extern const char* file_ext_str(uint i);
extern int file_format_detect(const void *data, ffsize len);

struct tag_edit {
	struct phi_tag_conf conf;
	fffd	fd, fdw;
	ffvec	buf;
	ffvec	meta2;
	fftime	mtime;
	char*	fnw;
	int		done;
};

static void tag_edit_close(struct tag_edit *t)
{
	if (t->done && t->conf.preserve_date) {
		fffile_set_mtime(t->fdw, &t->mtime);
	}
	fffile_close(t->fd);
	if (t->fd != t->fdw) {
		fffile_close(t->fdw);
		if (t->done && 0 != fffile_rename(t->fnw, t->conf.filename)) {
			syserrlog("file rename: %s -> %s", t->fnw, t->conf.filename);
			t->done = 0;
		}
	}
	if (t->done) {
		infolog("saved file %s", t->conf.filename);
	}
	ffmem_free(t->fnw);
	ffvec_free(&t->buf);
	ffvec_free(&t->meta2);
}

static int file_copydata(fffd src, ffuint64 offsrc, fffd dst, ffuint64 offdst, ffuint64 size)
{
	int rc = -1, r;
	ffvec v = {};
	ffvec_alloc(&v, 8*1024*1024, 1);

	while (size != 0) {
		if (0 > (r = fffile_readat(src, v.ptr, ffmin(size, v.cap), offsrc)))
			goto end;
		offsrc += r;
		if (0 > (r = fffile_writeat(dst, v.ptr, r, offdst)))
			goto end;
		offdst += r;
		size -= r;
	}

	rc = 0;

end:
	ffvec_free(&v);
	return rc;
}

static int user_meta_split(ffstr kv, ffstr *k, ffstr *v)
{
	if (0 > ffstr_splitby(&kv, '=', k, v)) {
		errlog("invalid meta: %S", &kv);
		return -1;
	}

	int tag;
	if (-1 == (tag = ffszarr_find(ffmmtag_str, FF_COUNT(ffmmtag_str), k->ptr, k->len)))
		return 0;
	return tag;
}

static int user_meta_find(const ffvec *m, uint tag, ffstr *k, ffstr *v)
{
	ffstr *kv;
	FFSLICE_WALK(m, kv) {
		if (kv->len) {
			int r = user_meta_split(*kv, k, v);
			if (r == (int)tag)
				return 1;
		}
	}
	return 0;
}

static int mp3_id3v2(struct tag_edit *t)
{
	int rc = 'e', r;
	uint id3v2_size = 0;
	struct id3v2write w = {};
	id3v2write_create(&w);
	w.as_is = 1;
	struct id3v2read id3v2 = {};
	id3v2read_open(&id3v2);
	id3v2.as_is = 1;

	if (0 > (r = fffile_readat(t->fd, t->buf.ptr, t->buf.cap, 0))) {
		syserrlog("file read: %s", t->conf.filename);
		goto end;
	}
	t->buf.len = r;

	ffstr *kv, in, k, v, ik, iv;

	ffstr_setstr(&in, &t->buf);
	int tag = id3v2read_process(&id3v2, &in, &ik, &iv);
	if (tag != ID3V2READ_NO) {
		id3v2_size = id3v2read_size(&id3v2);
		if (id3v2_size > t->buf.cap) {
			// we need full tag contents in memory
			if (id3v2_size > 100*1024*1024) {
				errlog("id3v2: %s: huge tag size %u", t->conf.filename, id3v2_size);
				goto end;
			}
			ffvec_realloc(&t->buf, id3v2_size, 1);
			if (0 > (r = fffile_readat(t->fd, t->buf.ptr, t->buf.cap, 0))) {
				syserrlog("file read: %s", t->conf.filename);
				goto end;
			}
			t->buf.len = r;

			id3v2read_close(&id3v2);
			ffmem_zero_obj(&id3v2);
			id3v2read_open(&id3v2);
			id3v2.as_is = 1;
			ffstr_setstr(&in, &t->buf);
			tag = id3v2read_process(&id3v2, &in, &ik, &iv);
		}
	}

	u_char tags_added[_MMTAG_N] = {};

	if (!t->conf.clear) {
		// replace tags, copy existing tags preserving the original order
		while (tag <= 0) {
			tag = -tag;
			if (user_meta_find(&t->conf.meta, tag, &k, &v)) {
				// write user tag
				dbglog("id3v2: writing %S = %S", &k, &v);
				id3v2write_add(&w, tag, v);

			} else {
				// copy existing tag
				dbglog("id3v2: writing %S = %S %d", &ik, &iv, tag);
				if (tag != 0) {
					id3v2write_add(&w, tag, iv);
				} else {
					// unknown tag
					char key[4] = {};
					ffmem_copy(key, ik.ptr, ffmax(ik.len, 4));
					_id3v2write_addframe(&w, key, FFSTR_Z(""), iv, 1);
				}
			}
			tags_added[tag] = 1;

			tag = id3v2read_process(&id3v2, &in, &ik, &iv);
			dbglog("id3v2: %d", tag);
			switch (tag) {
			case ID3V2READ_WARN:
			case ID3V2READ_ERROR:
				warnlog("%s", id3v2read_error(&id3v2));
			}
		}
	}

	// add new tags
	FFSLICE_WALK(&t->conf.meta, kv) {
		if (!kv->len)
			continue;
		tag = user_meta_split(*kv, &k, &v);
		if (tag < 0)
			goto end;
		if (tags_added[tag])
			continue; // already added
		dbglog("id3v2: writing %S = %S", &k, &v);
		id3v2write_add(&w, tag, v);
	}

	int padding = id3v2_size - w.buf.len;
	if (padding < 0)
		padding = 1000 - w.buf.len;
	if (padding < 0)
		padding = 0;
	if (0 != (r = id3v2write_finish(&w, padding))) {
		errlog("id3v2write_finish");
		goto end;
	}
	ffvec_free(&t->buf);
	t->buf = w.buf;
	ffvec_null(&w.buf);

	dbglog("id3v2: old size: %u, new size: %L", id3v2_size, t->buf.len);

	if (id3v2_size >= t->buf.len) {
		if (0 > (r = fffile_writeat(t->fd, t->buf.ptr, t->buf.len, 0))) {
			syserrlog("file write");
			goto end;
		}

	} else {
		t->fnw = ffsz_allocfmt("%s.phiolatemp", t->conf.filename);
		if (FFFILE_NULL == (t->fdw = fffile_open(t->fnw, FFFILE_CREATENEW | FFFILE_WRITEONLY))) {
			syserrlog("file create: %s", t->fnw);
			goto end;
		}
		dbglog("created file: %s", t->fnw);

		if (0 > (r = fffile_writeat(t->fdw, t->buf.ptr, t->buf.len, 0))) {
			syserrlog("file write:%s", t->fnw);
			goto end;
		}

		ffint64 sz = fffile_size(t->fd) - id3v2_size;
		if (0 != file_copydata(t->fd, id3v2_size, t->fdw, t->buf.len, sz)) {
			syserrlog("file read/write: %s -> %s", t->conf.filename, t->fnw);
			goto end;
		}
	}

	rc = 0;

end:
	id3v2read_close(&id3v2);
	id3v2write_close(&w);
	return rc;
}

static int mp3_id3v1(struct tag_edit *t)
{
	int r, have_id3v1 = 0;
	ffint64 sz = fffile_size(t->fd);
	ffint64 id3v1_off = sz - sizeof(struct id3v1);
	id3v1_off = ffmax(id3v1_off, 0);
	if (id3v1_off > 0) {
		if (0 > (r = fffile_readat(t->fd, t->buf.ptr, sizeof(struct id3v1), id3v1_off))) {
			syserrlog("file read");
			return 'e';
		}
		t->buf.len = r;
	}

	struct id3v1 w = {};
	id3v1write_init(&w);

	ffstr *kv, k, v, iv;

	struct id3v1read rd = {};
	rd.codepage = core->conf.code_page;
	ffstr id31_data = *(ffstr*)&t->buf;
	int tag = id3v1read_process(&rd, id31_data, &iv);
	if (tag != ID3V1READ_NO)
		have_id3v1 = 1;

	// add user tags
	FFSLICE_WALK(&t->conf.meta, kv) {
		if (!kv->len)
			continue;
		int tag = user_meta_split(*kv, &k, &v);
		if (tag < 0)
			return 'e';
		if (!tag)
			continue; // tag isn't supported in ID3v1
		r = id3v1write_set(&w, tag, v);
		if (r != 0)
			dbglog("id3v1: written %S = %S", &k, &v);
	}

	if (!t->conf.clear) {
		// copy existing tags
		while (tag < 0) {
			tag = -tag;
			if (!user_meta_find(&t->conf.meta, tag, &k, &v)) {
				int r2 = id3v1write_set(&w, tag, iv);
				if (r2 != 0)
					dbglog("id3v1: written %s = %S", ffmmtag_str[tag], &iv);
			}

			tag = id3v1read_process(&rd, id31_data, &iv);
		}
	}

	id3v1_off = fffile_size(t->fdw);
	if (have_id3v1)
		id3v1_off -= sizeof(struct id3v1);
	if (0 > (r = fffile_writeat(t->fdw, &w, sizeof(struct id3v1), id3v1_off))) {
		syserrlog("file write");
		return 'e';
	}
	return 0;
}

static int tag_edit_process(struct tag_edit *t)
{
	int r;

	if (FFFILE_NULL == (t->fd = fffile_open(t->conf.filename, FFFILE_READWRITE))) {
		syserrlog("file open: %s", t->conf.filename);
		return 'e';
	}
	t->fdw = t->fd;

	fffileinfo fi = {};
	if (0 != fffile_info(t->fd, &fi)){
		syserrlog("file info: %s", t->conf.filename);
		return 'e';
	}
	t->mtime = fffileinfo_mtime(&fi);

	ffvec_alloc(&t->buf, 1024, 1);
	if (0 > (r = fffile_read(t->fd, t->buf.ptr, t->buf.cap))) {
		syserrlog("file read");
		return 'e';
	}
	t->buf.len = r;

	uint fmt = file_format_detect(t->buf.ptr, t->buf.len);
	if (fmt == 0) {
		errlog("can't detect file format");
		return 'e';
	}

	const char *ext = file_ext_str(fmt);
	if (ffsz_eq(ext, "mp3")) {
		if (0 == (r = mp3_id3v2(t)))
			r = mp3_id3v1(t);
	} else {
		errlog("unsupported format");
		r = -1;
	}
	t->done = (r == 0);
	return r;
}

static int tag_edit_edit(struct phi_tag_conf *conf)
{
	struct tag_edit t = {
		.fd = FFFILE_NULL,
		.fdw = FFFILE_NULL,
		.conf = *conf,
	};
	int r = tag_edit_process(&t);
	tag_edit_close(&t);
	return r;
}

const phi_tag_if phi_tag = {
	tag_edit_edit
};
