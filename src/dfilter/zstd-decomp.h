/** phiola: zstd decompression filter
2023, Simon Zolin */

struct zstdr {
	zstd_decoder *zst;
	ffstr input;
	ffvec buf;
};

static void zstdr_close(void *ctx, phi_track *t)
{
	struct zstdr *z = ctx;
	zstd_decode_free(z->zst);
	ffvec_free(&z->buf);
	ffmem_free(z);
}

static void* zstdr_open(phi_track *t)
{
	struct zstdr *z = ffmem_new(struct zstdr);
	zstd_dec_conf zc = {};
	zstd_decode_init(&z->zst, &zc);
	ffvec_alloc(&z->buf, 512*1024, 1);
	return z;
}

static int zstdr_process(void *ctx, phi_track *t)
{
	struct zstdr *z = ctx;

	if (t->chain_flags & PHI_FFWD)
		z->input = t->data_in;

	zstd_buf in, out;
	zstd_buf_set(&in, z->input.ptr, z->input.len);
	zstd_buf_set(&out, z->buf.ptr, z->buf.cap);
	int r = zstd_decode(z->zst, &in, &out);
	ffstr_shift(&z->input, in.pos);
	if (r < 0) {
		errlog(t, "zstd_decode");
		return PHI_ERR;
	}

	if (out.pos == 0 && (t->chain_flags & PHI_FFIRST))
		return PHI_DONE;

	if (out.pos == 0 && r > 0)
		return PHI_MORE;

	ffstr_set(&t->data_out, z->buf.ptr, out.pos);
	return PHI_DATA;
}

const phi_filter phi_zstdr = {
	zstdr_open, zstdr_close, zstdr_process,
	"zstd-decomp"
};
