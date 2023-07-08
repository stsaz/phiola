/** phiola: file-write filter
2022, Simon Zolin */

#include <util/fcache.h>
#include <FFOS/file.h>

#define ALIGN (4*1024)

struct file_w {
	phi_track *trk;
	fffd fd;
	uint64 off_cur, size;
	struct fcache_buf wbuf;
	ffsize buf_cap;
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
	ffmem_free(f->filename_tmp);
	ffmem_free(f);
}

static void* fw_open(phi_track *t)
{
	struct file_w *f = ffmem_new(struct file_w);
	f->trk = t;
	f->fd = FFFILE_NULL;
	f->name = t->conf.ofile.name;
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
