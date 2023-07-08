/** phiola: zstd compression filter
2023, Simon Zolin */

struct zstdw {
	zstd_encoder *zst;
	ffstr input;
	ffvec buf;
};

static void zstdw_close(void *ctx, phi_track *t)
{
	struct zstdw *z = ctx;
	zstd_encode_free(z->zst);
	ffvec_free(&z->buf);
	ffmem_free(z);
}

static void* zstdw_open(phi_track *t)
{
	struct zstdw *z = ffmem_new(struct zstdw);
	zstd_enc_conf zc = {};
	zc.level = 1;
	zc.workers = 1;
	zstd_encode_init(&z->zst, &zc);
	ffvec_alloc(&z->buf, 512*1024, 1);
	return z;
}

static int zstdw_process(void *ctx, phi_track *t)
{
	struct zstdw *z = ctx;

	if (t->chain_flags & PHI_FFWD)
		z->input = t->data_in;

	zstd_buf in, out;
	zstd_buf_set(&in, z->input.ptr, z->input.len);
	zstd_buf_set(&out, z->buf.ptr, z->buf.cap);
	uint flags = 0;
	if (t->chain_flags & PHI_FFIRST)
		flags |= ZSTD_FFINISH;
	int r = zstd_encode(z->zst, &in, &out, flags);
	ffstr_shift(&z->input, in.pos);
	if (r < 0) {
		errlog(t, "zstd_encode");
		return PHI_ERR;
	}

	if (out.pos == 0 && (t->chain_flags & PHI_FFIRST))
		return PHI_DONE;

	if (out.pos == 0 && r > 0)
		return PHI_MORE;

	ffstr_set(&t->data_out, z->buf.ptr, out.pos);
	return PHI_DATA;
}

const phi_filter phi_zstdw = {
	zstdw_open, zstdw_close, zstdw_process,
	"zstd-comp"
};
