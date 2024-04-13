/** phiola: stdin file-read filter
2019, Simon Zolin */

#include <ffsys/std.h>
#include <ffsys/pipe.h>

struct std_in {
	fffd fd;
	uint64 total;
	ffvec buf;
};

static void stdin_close(void *ctx, phi_track *t)
{
	struct std_in *f = ctx;
	ffmem_alignfree(f->buf.ptr);
	phi_track_free(t, f);
}

static void* stdin_open(phi_track *t)
{
	struct std_in *f = phi_track_allocT(t, struct std_in);
	t->input.size = ~0ULL;
	t->input.seek = ~0ULL;
	f->fd = ffstdin;
	f->buf.cap = (t->conf.ifile.buf_size) ? t->conf.ifile.buf_size : 64*1024;
	f->buf.ptr = ffmem_align(f->buf.cap, 4*1024);
	return f;
}

static int stdin_read(void *ctx, phi_track *t)
{
	struct std_in *f = ctx;
	ssize_t r;
	uint64 seek = 0;
	ffstr buf;

	if (t->input.seek != ~0ULL) {
		uint64 off = f->total - f->buf.len;
		if (t->input.seek < off) {
			errlog(t, "can't seek backward on stdin.  offset:%U", t->input.seek);
			return PHI_ERR;
		}
		seek = t->input.seek;
		t->input.seek = ~0ULL;
		if (f->total > seek) {
			ffstr_set(&buf, f->buf.ptr, f->buf.len);
			ffstr_shift(&buf, seek - off);
			goto data;
		}
	}

	for (;;) {
		r = ffpipe_read(f->fd, f->buf.ptr, f->buf.cap);
		if (r == 0) {
			return PHI_DONE;
		} else if (r < 0) {
			syserrlog(t, "ffpipe_read");
			return PHI_ERR;
		}
		dbglog(t, "read %L bytes", r);
		f->total += r;
		f->buf.len = r;
		ffstr_set(&buf, f->buf.ptr, f->buf.len);
		if (seek == 0)
			break;
		else if (f->total > seek) {
			uint64 off = f->total - f->buf.len;
			ffstr_shift(&buf, seek - off);
			break;
		}
	}

data:
	t->data_out = buf;
	return PHI_DATA;
}

const phi_filter phi_stdin = {
	stdin_open, stdin_close, stdin_read,
	"stdin"
};
