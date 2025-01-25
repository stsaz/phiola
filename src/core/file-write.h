/** phiola: file-write filter
2022, Simon Zolin */

#include <util/fcache.h>
#include <util/util.h>
#include <ffsys/file.h>

#define ALIGN (4*1024)

struct file_w {
	phi_track*	trk;
	fffd		fd;
	uint64		off_cur, size;
	struct fcache bufs;

	ffstr		namebuf;
	const char*	name;
	char*		filename_tmp;

	phi_kevent*	kev;
	size_t		len_pending;
	uint64		offset_pending;
	ffstr		input; // input data yet to be processed
	fftime		t_start; // the time when we started blocking the track

	uint		buf_cap;
	uint		async :1; // expecting async signal
	uint		signalled :1; // async operation is ready
	uint		fin :1;

	struct {
		fftime t_open, t_io;
		uint64 writes, cached_writes;
	} stats;
};

static void fw_write_done(void *param);

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

	dbglog(t, "open:%Ums  io:%Ums/%U  cache-writes:%U"
		, fftime_to_msec(&f->stats.t_open)
		, fftime_to_msec(&f->stats.t_io), f->stats.writes
		, f->stats.cached_writes);

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
	core->kev_free(t->worker, f->kev);
	fcache_destroy(&f->bufs);
	ffstr_free(&f->namebuf);
	ffmem_free(f->filename_tmp);
	phi_track_free(t, f);
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
		VAR_DAY,
		VAR_FILENAME,
		VAR_FILEPATH,
		VAR_HOUR,
		VAR_MINUTE,
		VAR_MONTH,
		VAR_NOWDATE,
		VAR_NOWTIME,
		VAR_SECOND,
		VAR_YEAR,
	};
	static const char vars[][9] = {
		"counter",
		"day",
		"filename",
		"filepath",
		"hour",
		"minute",
		"month",
		"nowdate",
		"nowtime",
		"second",
		"year",
	};

	int r;
	ffstr val;
	ffdatetime dt = {};

	if (0 > (r = ffcharr_findsorted(vars, FF_COUNT(vars), sizeof(*vars), var.ptr+1, var.len-1))) {
		ffstr var_name = FFSTR_INITN(var.ptr+1, var.len-1);
		if (core->metaif->find(&t->meta, var_name, &val, 0))
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
	case VAR_YEAR:
	case VAR_MONTH:
	case VAR_DAY:
	case VAR_HOUR:
	case VAR_MINUTE:
	case VAR_SECOND: {
		if (!dt.year)
			core->time(&dt, PHI_CORE_TIME_LOCAL);
		uint n = 0;
		switch (r) {
		case VAR_NOWDATE:
			ffvec_addfmt(buf, "%04u%02u%02u", dt.year, dt.month, dt.day);  break;
		case VAR_NOWTIME:
			ffvec_addfmt(buf, "%02u%02u%02u", dt.hour, dt.minute, dt.second);  break;
		case VAR_YEAR:
			ffvec_addfmt(buf, "%04u", dt.year);  break;
		case VAR_MONTH:
			n = dt.month;  goto dt_add;
		case VAR_DAY:
			n = dt.day;  goto dt_add;
		case VAR_HOUR:
			n = dt.hour;  goto dt_add;
		case VAR_MINUTE:
			n = dt.minute;  goto dt_add;
		case VAR_SECOND:
			n = dt.second;
dt_add:
			ffvec_addfmt(buf, "%02u", n);  break;
		}
		return;
	}

	case VAR_COUNTER: {
		static uint counter;
		uint n = ffint_fetch_add(&counter, 1) + 1;
		ffvec_addfmt(buf, "%u", n);
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
	struct file_w *f = phi_track_allocT(t, struct file_w);
	f->trk = t;
	f->fd = FFFILE_NULL;
	f->name = fw_name(&f->namebuf, t->conf.ofile.name, t);
	f->buf_cap = (t->conf.ofile.buf_size) ? t->conf.ofile.buf_size : 64*1024;
	if (fcache_init(&f->bufs, 2, f->buf_cap, ALIGN))
		goto end;

	const char *fn = f->name;
	if (t->conf.ofile.name_tmp) {
		if (!t->conf.ofile.overwrite && fffile_exists(fn)) {
			t->error = PHI_E_DSTEXIST;
			errlog(t, "%s: file already exists", fn);
			goto end;
		}

		f->filename_tmp = ffsz_allocfmt("%s.tmp", fn);
		fn = f->filename_tmp;
	}

	if (NULL == (f->kev = core->kev_alloc(t->worker)))
		goto end;
	f->kev->kcall.handler = fw_write_done;
	f->kev->kcall.param = f;

	fftime t1;
	frw_benchmark(&t1);

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

	if (frw_benchmark(&f->stats.t_open))
		fftime_sub(&f->stats.t_open, &t1);

	ffmem_free(t->output.name);
	t->output.name = f->namebuf.ptr;
	ffstr_null(&f->namebuf);
	dbglog(t, "%s: opened", f->name);
	return f;

end:
	fw_close(f, t);
	return PHI_OPEN_ERR;
}

static void fw_write_done(void *param)
{
	struct file_w *f = param;
	FF_ASSERT(f->len_pending);
	f->signalled = 1;
	if (f->async) {
		f->async = 0;

		fftime t2;
		if (frw_benchmark(&t2)) {
			fftime_sub(&t2, &f->t_start);
			fftime_add(&f->stats.t_io, &t2);
		}

		core->track->wake(f->trk);
	}
}

/** Pass data to kernel */
static int fw_write(struct file_w *f, ffstr d, uint64 off, uint async)
{
	async &= f->trk->output.allow_async;

	ssize_t r;
	if (async) {
		r = fffile_writeat_async(f->fd, d.ptr, d.len, off, &f->kev->kcall);
	} else {
		fftime t1, t2;
		frw_benchmark(&t1);

		r = fffile_writeat(f->fd, d.ptr, d.len, off);

		if (frw_benchmark(&t2)) {
			fftime_sub(&t2, &t1);
			fftime_add(&f->stats.t_io, &t2);
		}
	}
	if (r < 0) {
		if (fferr_last() == FFKCALL_EINPROGRESS) {
			dbglog(f->trk, "file write: in progress");
			return -1;
		}
		syserrlog(f->trk, "file write: %s %L @%U", f->name, d.len, off);
		return 1;
	}

	dbglog(f->trk, "%s: written %L @%U", f->name, d.len, off);
	if (off + d.len > f->size)
		f->size = off + d.len;

	f->stats.writes++;
	return 0;
}

static int fw_process(void *ctx, phi_track *t)
{
	struct file_w *f = ctx;
	ffstr in = {}, d = {};

	FF_ASSERT(!(f->input.len && t->data_in.len));

	if (f->signalled) {
		f->signalled = 0;

		// get the result of async operation
		d.len = f->len_pending;
		f->len_pending = 0;
		if (fw_write(f, d, f->offset_pending, 1))
			return PHI_ERR;
	}

	in = t->data_in;
	t->data_in.len = 0;
	if (f->input.len) {
		in = f->input;
		f->input.len = 0;
	}

	uint64 off = f->off_cur;
	if (t->output.seek != ~0ULL) {
		off = t->output.seek;
		t->output.seek = ~0ULL;
		dbglog(t, "%s: seek @%U", f->name, off);
	}

	struct fcache_buf *b = fcache_curbuf(&f->bufs);
	for (;;) {

		size_t n = in.len,  blen = b->len;
		int64 woff = fbuf_write(b, f->buf_cap, &in, off, &d);
		off += n - in.len;
		if (b->len > blen) {
			f->stats.cached_writes++;
			dbglog(t, "%s: write: bufferred %L bytes @%U+%L"
				, f->name, n - in.len, b->off, b->len);
		}

		if (woff < 0) {
			if (t->chain_flags & PHI_FFIRST) {
				if (f->len_pending)
					goto async;

				d = fbuf_str(b);
				if (d.len != 0 && 0 != fw_write(f, d, b->off, 0))
					return PHI_ERR;
				f->fin = 1;
				return PHI_DONE;
			}
			break;
		}

		if (f->len_pending) {
			if (!b->len) {
				off -= n - in.len;
				in = d;
			}
			goto async;
		}

		int r = fw_write(f, d, woff, 1);
		if (r > 0)
			return PHI_ERR;
		if (r < 0) { // async
			f->len_pending = d.len;
			f->offset_pending = woff;
			if (b->len) {
				b->len = 0;
				b->off = 0;
				fcache_nextbuf(&f->bufs);
				b = fcache_curbuf(&f->bufs);
				// continue writing data to next buffer
			}

		} else {
			b->len = 0;
			b->off = 0;
		}
	}

	f->off_cur = off;
	return PHI_MORE;

async:
	frw_benchmark(&f->t_start);
	f->off_cur = off;
	f->input = in;
	f->async = 1;
	return PHI_ASYNC; // wait for pending operations to complete
}

const phi_filter phi_file_w = {
	fw_open, fw_close, fw_process,
	"file-write"
};

#undef ALIGN
