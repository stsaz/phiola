/** phiola: AAC ADTS (.aac) writer
2019, Simon Zolin */

struct aac_adts_w {
	uint state;
};

static void* aac_adts_w_open(phi_track *t)
{
	struct aac_adts_w *a = ffmem_new(struct aac_adts_w);
	return a;
}

static void aac_adts_w_close(void *ctx, phi_track *t)
{
	struct aac_adts_w *a = ctx;
	ffmem_free(a);
}

static int aac_adts_w_process(void *ctx, phi_track *t)
{
	struct aac_adts_w *a = ctx;

	switch (a->state) {
	case 0:
		if (!ffsz_eq(t->data_type, "aac")) {
			errlog(t, "unsupported data type: %s", t->data_type);
			return PHI_ERR;
		}
		// if (t->data_in.len != 0) {
		// skip ASC
		// }
		a->state = 1;
		return PHI_MORE;
	case 1:
		break;
	}

	if (t->data_in.len == 0 && !(t->chain_flags & PHI_FFIRST))
		return PHI_MORE;
	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_DATA;
}

const phi_filter phi_aac_adts_write = {
	aac_adts_w_open, (void*)aac_adts_w_close, (void*)aac_adts_w_process,
	"aac-adts-write"
};
