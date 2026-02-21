/** phiola: generate silence
2015, Simon Zolin */

#include <track.h>

#define SILGEN_BUF_MSEC 100

struct silgen {
	uint state;
	void *buf;
	size_t cap;
};

static void* silgen_open(phi_track *t)
{
	struct silgen *c = phi_track_allocT(t, struct silgen);
	return c;
}

static void silgen_close(void *ctx, phi_track *t)
{
	struct silgen *c = ctx;
	phi_track_free(t, c->buf);
	phi_track_free(t, c);
}

static int silgen_process(void *ctx, phi_track *t)
{
	struct silgen *c = ctx;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_DONE;
	}

	switch (c->state) {

	case 0:
		t->oaudio.conv_format = t->oaudio.format;
		t->data_type = PHI_AC_PCM;
		c->state = 1;
		return PHI_DATA;

	case 1:
		if (!t->oaudio.conv_format.interleaved) {
			errlog(t, "non-interleaved format is not supported");
			return PHI_ERR;
		}

		t->oaudio.format = t->oaudio.conv_format;
		c->cap = pcm_size1(&t->oaudio.format) * pcm_samples(SILGEN_BUF_MSEC, t->oaudio.format.rate);
		c->buf = phi_track_alloc(t, c->cap);
		c->state = 2;
		// fallthrough

	case 2:
		break;
	}

	ffstr_set(&t->data_out, c->buf, c->cap);
	return PHI_DATA;
}

const phi_filter phi_sil_gen = {
	silgen_open, silgen_close, silgen_process,
	"silence-gen"
};
