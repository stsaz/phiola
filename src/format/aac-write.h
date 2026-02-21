/** phiola: AAC ADTS (.aac) writer
2019, Simon Zolin */

#include <avpack/base/adts.h>

struct aac_adts_w {
	uint state;
	struct adts_hdr hdr;
	ffvec buf;
};

enum {
	I_ASC,
	I_DATA,
	I_COPY,
};

static void aac_adts_w_close(void *ctx, phi_track *t)
{
	struct aac_adts_w *a = ctx;
	ffvec_free(&a->buf);
	phi_track_free(t, a);
}

static void* aac_adts_w_open(phi_track *t)
{
	struct aac_adts_w *a = phi_track_allocT(t, struct aac_adts_w);
	if (t->data_type == PHI_AC_PCM) {
		if (!core->track->filter(t, core->mod("ac-aac.encode"), PHI_TF_PREV))
			goto err;
		a->hdr.sample_rate = t->oaudio.format.rate;
		a->hdr.chan_conf = t->oaudio.format.channels;
	} else if (t->data_type == PHI_AC_AAC) {
		a->state = I_COPY;
	} else {
		errlog(t, "input data format not supported: %u", t->data_type);
		goto err;
	}
	return a;

err:
	aac_adts_w_close(a, t);
	return PHI_OPEN_ERR;
}

static int aac_adts_w_process(void *ctx, phi_track *t)
{
	struct aac_adts_w *a = ctx;
	int r;

	switch (a->state) {
	case I_ASC:
		if (t->data_in.len == 0)
			return PHI_MORE;
		a->state = I_DATA;
		return PHI_MORE; // skip ASC

	case I_DATA:
		if (t->data_in.len == 0)
			return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_MORE;
		ffvec_grow(&a->buf, 7 + t->data_in.len, 1);
		r = adts_hdr_write(&a->hdr, a->buf.ptr, a->buf.cap, t->data_in.len);
		if (r == 0)
			return PHI_ERR;
		ffmem_copy(a->buf.ptr + r, t->data_in.ptr, t->data_in.len);
		a->buf.len = r + t->data_in.len;

		t->data_out = *(ffstr*)&a->buf;
		a->buf.len = 0;
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;

	case I_COPY:
		// if (t->data_in.len != 0) {
		// skip ASC
		// }
		if (t->data_in.len == 0 && !(t->chain_flags & PHI_FFIRST))
			return PHI_MORE;
		t->data_out = t->data_in;
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_DATA;
	}

	return PHI_ERR;
}

const phi_filter phi_aac_adts_write = {
	aac_adts_w_open, (void*)aac_adts_w_close, (void*)aac_adts_w_process,
	"aac-adts-write"
};
