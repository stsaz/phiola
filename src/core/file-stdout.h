/** phiola: stdout file-write filter
2019, Simon Zolin */

#include <track.h>

struct std_out {
	fffd fd;
	ffvec buf;
	uint64 fsize;
};

static void stdout_close(void *ctx, phi_track *t)
{
	struct std_out *f = ctx;
	ffvec_free(&f->buf);
	phi_track_free(t, f);
}

static void* stdout_open(phi_track *t)
{
	struct std_out *f = phi_track_allocT(t, struct std_out);
	f->fd = ffstdout;
	t->output.cant_seek = 1;
	ffvec_alloc(&f->buf, 64*1024, 1);
	return f;
}

static int stdout_writedata(struct std_out *f, const char *data, size_t len, phi_track *t)
{
	size_t r;
	r = fffile_write(f->fd, data, len);
	if (r != len) {
		syserrlog(t, "fffile_write");
		return -1;
	}

	dbglog(t, "written %L bytes at offset %U (%L pending)", r, f->fsize, t->data_in.len);
	f->fsize += r;
	return r;
}

static int stdout_write(void *ctx, phi_track *t)
{
	struct std_out *f = ctx;
	ssize_t r;
	ffstr dst;

	if (!(t->output.seek == ~0ULL || t->output.seek == 0)) {
		if (f->buf.len != 0) {
			if (-1 == stdout_writedata(f, f->buf.ptr, f->buf.len, t))
				return PHI_ERR;
			f->buf.len = 0;
		}

		errlog(t, "can't seek on stdout.  offset:%U", t->output.seek);
		return PHI_ERR;
	}

	t->data_out = t->data_in;
	for (;;) {

		r = ffstr_gather((ffstr*)&f->buf, &f->buf.cap, t->data_in.ptr, t->data_in.len, f->buf.cap, &dst);
		ffstr_shift(&t->data_in, r);
		if (dst.len == 0) {
			if (!(t->chain_flags & PHI_FFIRST) || f->buf.len == 0)
				break;
			ffstr_set(&dst, f->buf.ptr, f->buf.len);
		}
		f->buf.len = 0;

		if (-1 == stdout_writedata(f, dst.ptr, dst.len, t))
			return PHI_ERR;

		if (t->data_in.len == 0)
			break;
	}

	if (t->chain_flags & PHI_FFIRST) {
		return PHI_DONE;
	}

	return PHI_OK;
}

const phi_filter phi_stdout = {
	stdout_open, stdout_close, stdout_write,
	"stdout"
};
