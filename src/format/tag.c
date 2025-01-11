/** phiola: modify file tags in-place
2022, Simon Zolin */

#include <phiola.h>
#include <util/util.h>
#include <afilter/pcm.h>
#include <format/mmtag.h>
#include <avpack/id3v1.h>
#include <avpack/id3v2.h>
#include <avpack/ogg-read.h>
#include <avpack/ogg-write.h>
#include <avpack/vorbistag.h>
#include <avpack/opus-fmt.h>
#include <avpack/vorbis-fmt.h>
#include <avpack/flac-read.h>
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
	uint	written;
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
		infolog("%uKB written to file %s"
			, t->written / 1024, t->conf.filename);
	}
	ffmem_free(t->fnw);
	ffvec_free(&t->buf);
	ffvec_free(&t->meta2);
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

static int user_meta_find(const ffslice *m, uint tag, ffstr *k, ffstr *v)
{
	if (tag == MMTAG_UNKNOWN)
		return 0;

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

static int tag_file_write(struct tag_edit *t, ffstr head, ffstr tags, uint64 tail_off_src)
{
	uint64 src_tail_size = fffile_size(t->fd) - tail_off_src;

	t->fnw = ffsz_allocfmt("%s.phiolatemp", t->conf.filename);
	if (FFFILE_NULL == (t->fdw = fffile_open(t->fnw, FFFILE_CREATENEW | FFFILE_WRITEONLY))) {
		syserrlog("file create: %s", t->fnw);
		return -1;
	}
	fffile_trunc(t->fdw, head.len + tags.len + src_tail_size);
	dbglog("created file: %s", t->fnw);

	// Copy header
	if (head.len) {
		if (0 > fffile_writeat(t->fdw, head.ptr, head.len, 0)) {
			syserrlog("file write: %s", t->fnw);
			return -1;
		}
		dbglog("written %L bytes @%U", head.len, 0ULL);
	}

	// Write tags
	if (0 > fffile_writeat(t->fdw, tags.ptr, tags.len, head.len)) {
		syserrlog("file write: %s", t->fnw);
		return -1;
	}
	dbglog("written %L bytes @%U", tags.len, (uint64)head.len);

	// Copy tail
	uint64 woff = head.len + tags.len;
	if (file_copydata(t->fd, tail_off_src, t->fdw, woff, src_tail_size)) {
		syserrlog("file read/write: %s -> %s", t->conf.filename, t->fnw);
		return -1;
	}
	dbglog("written %U bytes @%U", src_tail_size, woff);

	t->written += head.len + tags.len + src_tail_size;
	return 0;
}

/**
Return whole ID3v2 region size */
static int tag_mp3_id3v2_read(struct tag_edit *t, ffstr hdr)
{
	int rc = -1, r;
	uint n = 0;
	ffstr k, v;
	struct id3v2read id3v2 = {};
	id3v2read_open(&id3v2);
	id3v2.as_is = 1;
	r = id3v2read_process(&id3v2, &hdr, &k, &v);
	if (r != ID3V2READ_NO) {
		n = id3v2read_size(&id3v2);
		if (n > t->buf.cap) {
			// we need full tag contents in memory
			if (n > 100*1024*1024) {
				errlog("id3v2: %s: huge tag size %u", t->conf.filename, n);
				goto end;
			}
			ffvec_realloc(&t->buf, n, 1);
			if (0 > (r = fffile_readat(t->fd, t->buf.ptr, t->buf.cap, 0))) {
				syserrlog("file read: %s", t->conf.filename);
				goto end;
			}
			t->buf.len = r;
		}
	}

	rc = n;

end:
	id3v2read_close(&id3v2);
	return rc;
}

static int tag_mp3_id3v2_process(struct tag_edit *t, struct id3v2write *w, ffstr in)
{
	int rc = -1, tag;
	ffstr *kv, k, v, ik, iv;
	struct id3v2read id3v2 = {
		.as_is = 1,
	};
	id3v2read_open(&id3v2);
	u_char tags_added[_MMTAG_N] = {};

	if (!t->conf.clear) {
		// replace tags, copy existing tags preserving the original order
		for (;;) {
			tag = id3v2read_process(&id3v2, &in, &ik, &iv);
			dbglog("id3v2: %d", tag);
			switch (tag) {
			case ID3V2READ_NO:
			case ID3V2READ_DONE:
				goto add;

			case ID3V2READ_WARN:
				warnlog("%s", id3v2read_error(&id3v2));
				continue;

			case ID3V2READ_ERROR:
				errlog("%s", id3v2read_error(&id3v2));
				goto end;

			default:
				if (tag > 0)
					goto end;
			}

			tag = -tag;
			if (user_meta_find(&t->conf.meta, tag, &k, &v)) {
				// write user tag
				dbglog("id3v2: writing %S = %S", &k, &v);
				if (id3v2write_add(w, tag, v)) {
					errlog("id3v2write_add()");
					goto end;
				}

			} else {
				// copy existing tag
				dbglog("id3v2: writing (%d) %S = %S ", tag, &ik, &iv);
				if (id3v2read_version(&id3v2) == 2) {
					if (tag == 0) {
						errlog("Can't copy ID3v2.2 tag %S", &ik);
						goto end;
					}
					if (id3v2write_add(w, tag, iv)) {
						errlog("id3v2write_add()");
						goto end;
					}

				} else {
					// Copy v3/v4 tag data as-is
					if (_id3v2write_addframe(w, ik.ptr, FFSTR_Z(""), iv, -1)) {
						errlog("_id3v2write_addframe()");
						goto end;
					}
				}
			}

			tags_added[tag] = 1;
		}
	}

add:

	// add new tags
	FFSLICE_WALK(&t->conf.meta, kv) {
		if (!kv->len)
			continue;
		tag = user_meta_split(*kv, &k, &v);
		if (tag < 0)
			goto end;
		if (tag == 0) {
			warnlog("skipping unknown tag: %S", &k);
			continue;
		}
		if (tags_added[tag])
			continue; // already added

		dbglog("id3v2: writing %S = %S", &k, &v);
		id3v2write_add(w, tag, v);
	}

	rc = 0;

end:
	id3v2read_close(&id3v2);
	return rc;
}

static int tag_mp3_id3v2(struct tag_edit *t)
{
	int rc = 'e', r;
	ffstr in;
	uint id3v2_size;
	struct id3v2write w = {};
	id3v2write_create(&w);
	w.as_is = 1;

	if (0 > (r = fffile_readat(t->fd, t->buf.ptr, t->buf.cap, 0))) {
		syserrlog("file read: %s", t->conf.filename);
		goto end;
	}
	t->buf.len = r;
	in = *(ffstr*)&t->buf;

	if ((int)(id3v2_size = tag_mp3_id3v2_read(t, in)) < 0)
		goto end;
	in = *(ffstr*)&t->buf;

	if (tag_mp3_id3v2_process(t, &w, in))
		goto end;

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
		t->written += r;

	} else {
		if (t->conf.no_expand) {
			errlog("File rewrite is disabled");
			goto end;
		}

		ffstr head = {};
		if (tag_file_write(t, head, *(ffstr*)&t->buf, id3v2_size))
			goto end;
	}

	rc = 0;

end:
	id3v2write_close(&w);
	return rc;
}

static int tag_mp3_id3v1(struct tag_edit *t)
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
	t->written += r;
	return 0;
}

/** Read Vorbis tags region.
Require Vorbis/Opus info packet on a separate page;
 require tags packet on a separate page,
 but allow tags packet on the same page with Vorbis Codebook.
vtags: tags body (including framing bit for Vorbis)
vcb: full Vorbis Codebook packet
Return packet format: 'o', 'v' */
static int tag_ogg_vtag_read(oggread *ogg, ffstr in, ffstr *page, uint *page_off, uint *page_num, ffstr *vtags, ffstr *vcb)
{
	enum {
		I_HDR, I_OPUS_TAGS, I_VORBIS_TAGS, I_VORBIS_CB,
	};
	uint state = 0;
	for (;;) {
		ffstr pkt;
		int r = oggread_process(ogg, &in, &pkt);
		switch (r) {
		case OGGREAD_HEADER:
			switch (state) {
			case I_HDR:
				if (ogg->page_counter != 1)
					goto err;
				if (pkt.len >= 7
					&& !ffmem_cmp(pkt.ptr, "\x01vorbis", 7))
					state = I_VORBIS_TAGS;
				else if (pkt.len >= 8
					&& !ffmem_cmp(pkt.ptr, "OpusHead", 8))
					state = I_OPUS_TAGS;
				else
					goto err;
				continue;

			case I_OPUS_TAGS:
				if (ogg->page_counter != 2)
					goto err;
				if (!(r = opus_tags_read(pkt.ptr, pkt.len)))
					goto err;
				ffstr_shift(&pkt, r);
				*vtags = pkt;
				break;

			case I_VORBIS_TAGS:
				if (ogg->page_counter != 2)
					goto err;
				if (!(r = vorbis_tags_read(pkt.ptr, pkt.len)))
					goto err;
				ffstr_shift(&pkt, r);
				*vtags = pkt;
				if (oggread_pkt_last(ogg))
					break;
				state = I_VORBIS_CB;
				continue;

			case I_VORBIS_CB:
				if (!(pkt.len >= 7
					&& !ffmem_cmp(pkt.ptr, "\x05vorbis", 7)))
					goto err;
				*vcb = pkt;
				break;
			}

			if (!oggread_pkt_last(ogg))
				goto err;
			*page = ogg->chunk;
			*page_off = ogg->off - _avp_stream_used(&ogg->stream);
			*page_num = oggread_page_num(ogg);
			return (state == I_OPUS_TAGS) ? 'o' : 'v';

		case OGGREAD_MORE:
			errlog("couldn't find Vorbis Tags");
			break;

		default:
			errlog("ogg parser returned code %d", r);
		}
		break;
	}

err:
	errlog("file format not supported");
	return 0;
}

static int tag_opus_r128_track_gain(vorbistagwrite *vtw, ffstr v)
{
	int r;
	double d;
	if (!ffstr_to_float(&v, &d)
		|| Q78_from_float(&r, RG_R128(d))) {
		warnlog("incorrect value: %S", &v);
		return -1;
	}

	char val[8];
	v.ptr = val;
	v.len = ffs_fromint(r, val, sizeof(val), FFS_INTSIGN);
	ffstr k = FFSTR_Z("R128_TRACK_GAIN");
	if (!vorbistagwrite_add_name(vtw, k, v))
		dbglog("vorbistag: written %S = %S", &k, &v);
	return 0;
}

static int tag_ogg_process(struct tag_edit *t, vorbistagwrite *vtw, ffstr vtag, uint format)
{
	ffstr *kv, k, v, k2, v2;
	vorbistagread vtr = {};
	u_char tags_added[_MMTAG_N] = {};

	// Copy "Vendor" field
	int tag = vorbistagread_process(&vtr, &vtag, &k, &v);
	if (tag != MMTAG_VENDOR) {
		errlog("parsing Vorbis tag");
		return -1;
	}
	if (!vorbistagwrite_add(vtw, MMTAG_VENDOR, v))
		dbglog("vorbistag: written vendor = %S", &v);
	tags_added[MMTAG_VENDOR] = 1;

	if (!t->conf.clear) {
		// Replace tags, copy existing tags preserving the original order
		for (;;) {
			tag = vorbistagread_process(&vtr, &vtag, &k, &v);
			if (tag == VORBISTAGREAD_DONE) {
				break;
			} else if (tag == VORBISTAGREAD_ERROR) {
				errlog("parsing Vorbis tags");
				return -1;
			}

			if (user_meta_find(&t->conf.meta, tag, &k2, &v2)) {
				if (tag == MMTAG_REPLAYGAIN_TRACK_GAIN && format == 'o')
					continue; // Skip existing REPLAYGAIN_TRACK_GAIN tag

				// Write user tag
				if (!vorbistagwrite_add(vtw, tag, v2))
					dbglog("vorbistag: written %S = %S", &k2, &v2);

			} else {
				// Copy existing tag
				if (tag != 0) {
					vorbistagwrite_add(vtw, tag, v);
				} else {
					// Unknown tag
					vorbistagwrite_add_name(vtw, k, v);
				}
			}

			tags_added[tag] = 1;
		}
	}

	// Add new tags
	FFSLICE_WALK(&t->conf.meta, kv) {
		if (!kv->len)
			continue;
		tag = user_meta_split(*kv, &k, &v);
		if (tag < 0)
			return -1;
		if (!tag)
			continue;
		if (tags_added[tag])
			continue;

		if (tag == MMTAG_REPLAYGAIN_TRACK_GAIN && format == 'o') {
			// Write R128_TRACK_GAIN tag instead of REPLAYGAIN_TRACK_GAIN
			tag_opus_r128_track_gain(vtw, v);
			continue;
		}

		if (!vorbistagwrite_add(vtw, tag, v))
			dbglog("vorbistag: written %S = %S", &k, &v);
	}

	return 0;
}

static int tag_ogg(struct tag_edit *t)
{
	int r, rc = 'e', format;
	uint tags_page_off, tags_page_num, vtags_len;
	ffstr vtag = {}, vorbis_codebook = {}, page;
	oggwrite ogw = {};
	oggread ogg = {};
	vorbistagwrite vtw = {
		.left_zone = 8,
	};
	vorbistagwrite_create(&vtw);

	ffvec_realloc(&t->buf, 64*1024, 1);
	if (0 > (r = fffile_readat(t->fd, t->buf.ptr, t->buf.cap, 0))) {
		syserrlog("file read");
		return 'e';
	}
	t->buf.len = r;

	oggread_open(&ogg, -1);
	if (!(format = tag_ogg_vtag_read(&ogg, *(ffstr*)&t->buf, &page, &tags_page_off, &tags_page_num, &vtag, &vorbis_codebook)))
		goto end;
	vtags_len = vtag.len - ((format == 'v') ? 1 : 0);

	if (tag_ogg_process(t, &vtw, vtag, format))
		goto end;

	// Prepare OGG packet with Opus/Vorbis header, tags data and padding
	uint tags_len = vorbistagwrite_fin(&vtw).len;
	int padding = vtags_len - tags_len;
	if (padding < 0)
		padding = 1000 - tags_len;
	if (padding < 0)
		padding = 0;
	ffvec_grow(&vtw.out, 1 + padding, 1);
	ffstr vt = *(ffstr*)&vtw.out;
	if (format == 'o') {
		vt.len = opus_tags_write(vt.ptr, -1, tags_len);
	} else {
		ffstr_shift(&vt, 1);
		vt.len = vorbis_tags_write(vt.ptr, -1, tags_len);
	}
	ffmem_zero(vt.ptr + vt.len, padding);
	vt.len += padding;

	// Prepare OGG page with Vorbis Tags and maybe Vorbis Codebook
	ogw.page.number = tags_page_num;
	oggwrite_create(&ogw, ogg.info.serial, 0);
	ffstr wpage;
	uint f = (vorbis_codebook.len) ? 0 : OGGWRITE_FFLUSH;
	r = oggwrite_process(&ogw, &vt, &wpage, 0, f);
	if (vt.len) {
		errlog("resulting OGG page is too large");
		goto end;
	}
	if (vorbis_codebook.len) {
		r = oggwrite_process(&ogw, &vorbis_codebook, &wpage, 0, OGGWRITE_FFLUSH);
		if (vorbis_codebook.len) {
			errlog("resulting OGG page is too large");
			goto end;
		}
	}
	FF_ASSERT(r == OGGWRITE_DATA);
	FF_ASSERT(wpage.len >= page.len);

	if (wpage.len == page.len) {
		// Rewrite OGG page in-place
		if (0 > (r = fffile_writeat(t->fdw, wpage.ptr, wpage.len, tags_page_off))) {
			syserrlog("file write");
			goto end;
		}
		t->written += r;
		dbglog("written %L bytes @%U", wpage.len, tags_page_off);

	} else {
		if (t->conf.no_expand) {
			errlog("File rewrite is disabled");
			goto end;
		}

		ffstr head = FFSTR_INITN(t->buf.ptr, tags_page_off);
		if (tag_file_write(t, head, wpage, tags_page_off + page.len))
			goto end;
	}

	rc = 0;

end:
	oggwrite_close(&ogw);
	vorbistagwrite_destroy(&vtw);
	oggread_close(&ogg);
	return rc;
}

static int tag_flac_process(struct tag_edit *t, vorbistagwrite *vtw, ffstr vtags)
{
	ffstr *kv, k, v, k2, v2;
	vorbistagread vtr = {};
	u_char tags_added[_MMTAG_N] = {};

	// Copy "Vendor" field
	int tag = vorbistagread_process(&vtr, &vtags, &k, &v);
	if (tag != MMTAG_VENDOR) {
		errlog("parsing Vorbis tag");
		return -1;
	}
	if (!vorbistagwrite_add(vtw, MMTAG_VENDOR, v))
		dbglog("vorbistag: written vendor = %S", &v);
	tags_added[MMTAG_VENDOR] = 1;

	if (!t->conf.clear) {
		// Replace tags, copy existing tags preserving the original order
		for (;;) {
			tag = vorbistagread_process(&vtr, &vtags, &k, &v);
			if (tag == VORBISTAGREAD_DONE) {
				break;
			} else if (tag == VORBISTAGREAD_ERROR) {
				errlog("parsing Vorbis tags");
				return -1;
			}

			if (user_meta_find(&t->conf.meta, tag, &k2, &v2)) {
				// Write user tag
				if (!vorbistagwrite_add(vtw, tag, v2))
					dbglog("vorbistag: written %S = %S", &k2, &v2);

			} else {
				// Copy existing tag
				if (tag != 0) {
					vorbistagwrite_add(vtw, tag, v);
				} else {
					// Unknown tag
					vorbistagwrite_add_name(vtw, k, v);
				}
			}

			tags_added[tag] = 1;
		}
	}

	// Add new tags
	FFSLICE_WALK(&t->conf.meta, kv) {
		if (!kv->len)
			continue;
		tag = user_meta_split(*kv, &k, &v);
		if (tag < 0)
			return -1;
		if (!tag)
			continue;
		if (tags_added[tag])
			continue;

		if (!vorbistagwrite_add(vtw, tag, v))
			dbglog("vorbistag: written %S = %S", &k, &v);
	}

	return 0;
}

static int tag_flac(struct tag_edit *t)
{
	int rc = 'e', r;
	ffstr input = {}, output;
	uint64 tags_hdr_off = 0, padding_hdr_off = 0;
	uint vtags_len = 0, padding_len, padding_last;
	vorbistagwrite vtw = {
		.left_zone = sizeof(struct flac_hdr),
	};
	vorbistagwrite_create(&vtw);
	flacread fr = {};
	flacread_open(&fr, 0);

	ffvec_realloc(&t->buf, 64*1024, 1);
	input = *(ffstr*)&t->buf;

	for (;;) {
		r = flacread_process(&fr, &input, &output);
		switch (r) {
		case FLACREAD_MORE:
			if (0 >= (r = fffile_read(t->fd, t->buf.ptr, t->buf.cap))) {
				if (r == 0)
					errlog("bad FLAC file");
				else
					syserrlog("file read");
				goto end;
			}
			t->buf.len = r;
			input = *(ffstr*)&t->buf;
			break;

		case FLACREAD_HEADER:
			break;

		case FLACREAD_META_BLOCK:
			dbglog("meta block %u", flacread_meta_type(&fr));
			switch (flacread_meta_type(&fr)) {
			case FLAC_TTAGS:
				vtags_len = output.len;
				tags_hdr_off = flacread_meta_offset(&fr);
				if (tag_flac_process(t, &vtw, output))
					goto end;
				break;

			case FLAC_TPADDING:
				padding_last = fr.last_hdr_block;
				padding_len = output.len;
				padding_hdr_off = flacread_meta_offset(&fr);
				goto fin;
			}
			break;

		case FLACREAD_HEADER_FIN:
			goto fin;

		default:
			errlog("flacread_process(): %u", r);
			goto end;
		}
	}

fin:
	dbglog("tags @%U  padding @%U", tags_hdr_off, padding_hdr_off);
	if (!tags_hdr_off || !padding_hdr_off) {
		errlog("Tags and padding regions must already exist");
		goto end;
	}
	if (tags_hdr_off + sizeof(struct flac_hdr) + vtags_len != padding_hdr_off) {
		errlog("Tags region must preceed padding region");
		goto end;
	}

	uint new_tags_len = vorbistagwrite_fin(&vtw).len;
	dbglog("old tags size:%u  new size:%u", vtags_len, new_tags_len);

	int padding = vtags_len + padding_len - new_tags_len;
	if (padding < 0)
		padding = 0;
	ffvec_grow(&vtw.out, sizeof(struct flac_hdr) + padding, 1);
	ffstr vt = *(ffstr*)&vtw.out;

	flac_hdr_write(vt.ptr, FLAC_TTAGS, 0, new_tags_len);

	vt.len += flac_hdr_write(vt.ptr + vt.len, FLAC_TPADDING, padding_last, padding);
	ffmem_zero(vt.ptr + vt.len, padding);
	vt.len += padding;

	if (vt.len == sizeof(struct flac_hdr) * 2 + vtags_len + padding_len) {
		// Rewrite tags and padding regions in-place
		if (0 > (r = fffile_writeat(t->fdw, vt.ptr, vt.len, tags_hdr_off))) {
			syserrlog("file write");
			goto end;
		}
		t->written += r;
		dbglog("written %L bytes @%U", vt.len, tags_hdr_off);

	} else {
		if (t->conf.no_expand) {
			errlog("File rewrite is disabled");
			goto end;
		}

		errlog("Expanding FLAC files is not implemented");
		goto end;
	}

	rc = 0;

end:
	vorbistagwrite_destroy(&vtw);
	flacread_close(&fr);
	return rc;
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
	dbglog("%s: %s", t->conf.filename, ext);
	if (ffsz_eq(ext, "mp3")) {
		if (0 == (r = tag_mp3_id3v2(t)))
			r = tag_mp3_id3v1(t);

	} else if (ffsz_eq(ext, "ogg")) {
		r = tag_ogg(t);

	} else if (ffsz_eq(ext, "flac")) {
		r = tag_flac(t);

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
