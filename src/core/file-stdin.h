/** phiola: stdin file-read filter
2019, Simon Zolin */

#include <FFOS/std.h>
#include <FFOS/pipe.h>

struct std_in {
	fffd fd;
	uint64 total;
	ffvec buf;
};

static void stdin_close(void *ctx, phi_track *t)
{
	struct std_in *f = ctx;
	ffvec_free(&f->buf);
	ffmem_free(f);
}

static void* stdin_open(phi_track *t)
{
	struct std_in *f = ffmem_new(struct std_in);
	t->input.size = ~0ULL;
	t->input.seek = ~0ULL;
	f->fd = ffstdin;
	ffvec_alloc(&f->buf, 64*1024, 1);
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
