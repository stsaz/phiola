/** phiola: file-write filter
2022, Simon Zolin */

#include <util/fcache.h>
#include <util/util.h>
#include <ffsys/file.h>

#define ALIGN (4*1024)

struct file_w {
	phi_track *trk;
	fffd fd;
	uint64 off_cur, size;
	struct fcache_buf wbuf;
	ffsize buf_cap;
	ffstr namebuf;
	const char *name;
	char *filename_tmp;
	uint fin;
};

static void fw_close(void *ctx, phi_track *t)
{
	struct file_w *f = ctx;

	if (f->fd == FFFILE_NULL)
		goto end;

	fffile_trunc(f->fd, f->size);

	if (0 != fffile_close(f->fd)) {
		syserrlog(t, "file close: %s", f->name);
		goto end;
	}

	if (!f->fin) {
		if (0 == fffile_remove(f->name))
			dbglog(t, "removed file %s", f->name);
		goto end;
	}

	if (t->conf.ofile.mtime.sec != 0) {
		fftime mt = t->conf.ofile.mtime;
		mt.sec -= FFTIME_1970_SECONDS;
		fffile_set_mtime_path(f->name, &mt);
	}

	if (f->filename_tmp != NULL
		&& 0 != fffile_rename(f->filename_tmp, f->name)) {
		syserrlog(t, "fffile_rename: %s -> %s", f->filename_tmp, f->name);
		goto end;
	}

	infolog(t, "%s: written %UKB", f->name, ffint_align_ceil2(f->size, 1024) / 1024);

end:
	ffmem_alignfree(f->wbuf.ptr);
	ffstr_free(&f->namebuf);
	ffmem_free(f->filename_tmp);
	ffmem_free(f);
}

/** All printable, plus SPACE, except: ", *, /, :, <, >, ?, \, | */
static const uint _ffpath_charmask_filename[] = {
	0,
	            // ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!
	0x2bff7bfb, // 0010 1011 1111 1111  0111 1011 1111 1011
	            // _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@
	0xefffffff, // 1110 1111 1111 1111  1111 1111 1111 1111
	            //  ~}| {zyx wvut srqp  onml kjih gfed cba`
	0x6fffffff, // 0110 1111 1111 1111  1111 1111 1111 1111
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff
};

static void fw_name_var(ffvec *buf, ffstr var, phi_track *t)
{
	enum VARS {
		VAR_COUNTER,
		VAR_FILENAME,
		VAR_FILEPATH,
		VAR_NOWDATE,
		VAR_NOWTIME,
	};
	static const char vars[][9] = {
		"counter",
		"filename",
		"filepath",
		"nowdate",
		"nowtime",
	};

	int r;
	ffstr val;
	ffdatetime dt = {};

	if (0 > (r = ffcharr_findsorted(vars, FF_COUNT(vars), sizeof(*vars), var.ptr+1, var.len-1))) {

		static const phi_meta_if *metaif;
		if (!metaif)
			metaif = core->mod("format.meta");

		ffstr var_name = FFSTR_INITN(var.ptr+1, var.len-1);
		if (!!metaif->find(&t->meta, var_name, &val, 0))
			val = var;
		goto data;
	}

	switch (r) {
	case VAR_FILEPATH:
	case VAR_FILENAME: {
		ffstr fdir = {}, fname = {};
		ffpath_split3_str(FFSTR_Z(t->conf.ifile.name), &fdir, &fname, NULL);
		val = (r == VAR_FILEPATH) ? fdir : fname;
		ffvec_addstr(buf, &val);
		return;
	}

	case VAR_NOWDATE:
	case VAR_NOWTIME:
		if (!dt.year)
			core->time(&dt, 0);
		if (r == VAR_NOWDATE)
			ffvec_addfmt(buf, "%04u%02u%02u", dt.year, dt.month, dt.day);
		else
			ffvec_addfmt(buf, "%02u%02u%02u", dt.hour, dt.minute, dt.second);
		return;

	case VAR_COUNTER: {
		static uint counter;
		ffvec_addfmt(buf, "%u", ++counter);
		return;
	}
	}

data:
	ffvec_grow(buf, val.len, 1);
	buf->len += ffpath_makefn(ffslice_end(buf, 1), -1, val, '_', _ffpath_charmask_filename);
}

static const char* fw_name(ffstr *sbuf, const char *name, phi_track *t)
{
	ffstr fn = FFSTR_INITZ(name);
	ffvec buf = {}, fnbuf = {};

	// "PATH/.EXT" -> "PATH/@filename.EXT"
	ffstr fdir, fname, ext;
	ffpath_split3_output(fn, &fdir, &fname, &ext);
	if (!fname.len) {
		ffvec_addfmt(&fnbuf, "%S%*c@filename.%S"
			, &fdir, (fdir.len) ? (ffsize)1 : (ffsize)0, FFPATH_SLASH, &ext);
		ffstr_set2(&fn, &fnbuf);

	} else if (ffstr_findchar(&fn, '@') < 0) {
		return name; // no variables to expand
	}

	while (fn.len) {
		ffstr val;
		int r = ffstr_var_next(&fn, &val, '@');
		if (r == 'v')
			fw_name_var(&buf, val, t);
		else
			ffvec_addstr(&buf, &val);
	}

	ffvec_addchar(&buf, '\0');
	ffstr_set2(sbuf, &buf);
	ffvec_null(&buf);
	sbuf->len--;

	ffvec_free(&fnbuf);
	return sbuf->ptr;
}

static void* fw_open(phi_track *t)
{
	struct file_w *f = ffmem_new(struct file_w);
	f->trk = t;
	f->fd = FFFILE_NULL;
	f->name = fw_name(&f->namebuf, t->conf.ofile.name, t);
	f->buf_cap = (t->conf.ofile.buf_size) ? t->conf.ofile.buf_size : 64*1024;

	f->wbuf.ptr = ffmem_align(f->buf_cap, ALIGN);

	const char *fn = f->name;
	if (t->conf.ofile.name_tmp) {
		if (!t->conf.ofile.overwrite && fffile_exists(fn)) {
			errlog(t, "%s: file already exists", fn);
			goto end;
		}

		f->filename_tmp = ffsz_allocfmt("%s.tmp", fn);
		fn = f->filename_tmp;
	}

	uint flags = FFFILE_WRITEONLY;
	if (t->conf.ofile.overwrite)
		flags |= FFFILE_CREATE;
	else
		flags |= FFFILE_CREATENEW;
	if (FFFILE_NULL == (f->fd = fffile_open(fn, flags))) {
		t->error = PHI_E_SYS | fferr_last();
		if (fferr_exist(fferr_last()))
			t->error = PHI_E_DSTEXIST;
		syserrlog(t, "fffile_open: %s", fn);
		goto end;
	}

	dbglog(t, "%s: opened", f->name);
	return f;

end:
	fw_close(f, t);
	return PHI_OPEN_ERR;
}

/** Pass data to kernel */
static int fw_write(struct file_w *f, ffstr d, uint64 off)
{
	ffssize r = fffile_writeat(f->fd, d.ptr, d.len, off);
	if (r < 0) {
		syserrlog(f->trk, "file write: %s %L @%U", f->name, d.len, off);
		return 1;
	}
	dbglog(f->trk, "%s: written %L @%U", f->name, d.len, off);
	if (off + d.len > f->size)
		f->size = off + d.len;
	return 0;
}

static int fw_process(void *ctx, phi_track *t)
{
	struct file_w *f = ctx;

	uint64 off = f->off_cur;
	if (t->output.seek != ~0ULL) {
		off = t->output.seek;
		t->output.seek = ~0ULL;
		dbglog(t, "%s: seek @%U", f->name, off);
	}

	ffstr in = t->data_in;
	for (;;) {
		ffstr d;
		ffsize n = in.len;
		int64 woff = fbuf_write(&f->wbuf, f->buf_cap, &in, off, &d);
		off += n - in.len;
		if (n != in.len) {
			dbglog(t, "%s: write: bufferred %L bytes @%U+%L"
				, f->name, n - in.len, f->wbuf.off, f->wbuf.len);
		}

		if (woff < 0) {
			if (t->chain_flags & PHI_FFIRST) {
				ffstr d;
				ffstr_set(&d, f->wbuf.ptr, f->wbuf.len);
				if (d.len != 0 && 0 != fw_write(f, d, f->wbuf.off))
					return PHI_ERR;
				f->fin = 1;
				return PHI_DONE;
			}
			break;
		}

		if (0 != fw_write(f, d, woff))
			return PHI_ERR;

		f->wbuf.len = 0;
		f->wbuf.off = 0;
	}

	f->off_cur = off;
	return PHI_MORE;
}

const phi_filter phi_file_w = {
	fw_open, fw_close, fw_process,
	"file-write"
};

#undef ALIGN
