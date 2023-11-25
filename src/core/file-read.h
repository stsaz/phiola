/** phiola: file-read filter
2022, Simon Zolin */

#include <util/fcache.h>
#include <ffsys/file.h>

#define ALIGN (4*1024)

struct file_r {
	fffd fd;
	uint64 off_cur;
	ffsize buf_cap;
	struct fcache fcache;
	uint eof :1;

	struct {
		fftime t_open, t_io;
		uint64 io_bytes;
	} stats;
};

static void fr_close(struct file_r *f, phi_track *t)
{
	dbglog(t, "open:%Ums  io:%Ums;%UKB/%U  cache-hits:%U"
		, fftime_to_msec(&f->stats.t_open)
		, fftime_to_msec(&f->stats.t_io), f->stats.io_bytes, f->fcache.misses
		, f->fcache.hits);
	fcache_destroy(&f->fcache);
	fffile_close(f->fd);
	ffmem_free(f);
}

static void* fr_open(phi_track *t)
{
	struct file_r *f = ffmem_new(struct file_r);
	f->buf_cap = (t->conf.ifile.buf_size) ? t->conf.ifile.buf_size : 64*1024;
	f->fd = FFFILE_NULL;
	if (0 != fcache_init(&f->fcache, 2, f->buf_cap, ALIGN))
		goto end;

	fftime t1;
	frw_benchmark(&t1);

	if (FFFILE_NULL == (f->fd = fffile_open(t->conf.ifile.name, FFFILE_READONLY))) {
		t->error = PHI_E_SYS | fferr_last();
		if (fferr_notexist(fferr_last()))
			t->error = PHI_E_NOSRC;
		syserrlog(t, "fffile_open: %s", t->conf.ifile.name);
		goto end;
	}

	fffileinfo fi;
	if (0 != fffile_info(f->fd, &fi)) {
		syserrlog(t, "fffile_info: %s", t->conf.ifile.name);
		goto end;
	}
	t->input.size = fffileinfo_size(&fi);
	t->input.mtime = fffileinfo_mtime(&fi);
	t->input.mtime.sec += FFTIME_1970_SECONDS;
	if (t->conf.ifile.preserve_date)
		t->conf.ofile.mtime = t->input.mtime;

	if (frw_benchmark(&f->stats.t_open))
		fftime_sub(&f->stats.t_open, &t1);

	dbglog(t, "%s: opened (%U kbytes)"
		, t->conf.ifile.name, t->input.size / 1024);
	return f;

end:
	fr_close(f, t);
	return PHI_OPEN_ERR;
}

static int fr_process(struct file_r *f, phi_track *t)
{
	if (t->input.seek != ~0ULL) {
		f->off_cur = t->input.seek;
		t->input.seek = ~0ULL;
		dbglog(t, "%s: seek @%U", t->conf.ifile.name, f->off_cur);
	}
	uint64 off = f->off_cur;

	struct fcache_buf *b;
	if (NULL != (b = fcache_find(&f->fcache, off))) {
		dbglog(t, "%s: cache hit: %L @%U", t->conf.ifile.name, b->len, b->off);
		goto done;
	}

	b = fcache_nextbuf(&f->fcache);

	fftime t1, t2;
	frw_benchmark(&t1);

	ffuint64 off_al = ffint_align_floor2(off, ALIGN);
	ffssize r = fffile_readat(f->fd, b->ptr, f->buf_cap, off_al);
	if (r < 0) {
		syserrlog(t, "%s: read", t->conf.ifile.name);
		return PHI_ERR;
	}
	b->len = r;
	b->off = off_al;
	dbglog(t, "%s: read: %L @%U", t->conf.ifile.name, b->len, b->off);

	if (frw_benchmark(&t2)) {
		fftime_sub(&t2, &t1);
		fftime_add(&f->stats.t_io, &t2);
		f->stats.io_bytes += r;
	}

done:
	f->off_cur = b->off + b->len;
	ffstr_set(&t->data_out, b->ptr, b->len);
	ffstr_shift(&t->data_out, ffmin(t->data_out.len, off - b->off));
	if (f->eof && t->data_out.len == 0)
		return PHI_DONE;
	f->eof = (t->data_out.len == 0);
	return PHI_DATA;
}

const phi_filter phi_file_r = {
	fr_open, (void*)fr_close, (void*)fr_process,
	"file-read"
};

#undef ALIGN
