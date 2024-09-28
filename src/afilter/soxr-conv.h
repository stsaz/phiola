/** phiola: afilter: soxr sample rate convertor
2023, Simon Zolin */

#include <track.h>
#include <util/aformat.h>
#include <afilter/soxr.h>

struct soxr {
	uint state;
	ffsoxr soxr;
	struct phi_af inpcm, outpcm;
};

static void* soxr_open(phi_track *t)
{
	struct soxr *c = phi_track_allocT(t, struct soxr);
	c->inpcm = t->aconv.in;
	c->outpcm = t->aconv.out;
	ffsoxr_init(&c->soxr);
	return c;
}

static void soxr_close(void *ctx, phi_track *t)
{
	struct soxr *c = ctx;
	ffsoxr_destroy(&c->soxr);
	phi_track_free(t, c);
}

static void log_pcmconv(int r, const struct phi_af *in, const struct phi_af *out, phi_track *t)
{
	int level = PHI_LOG_DEBUG;
	const char *unsupp = "";
	if (r != 0) {
		level = PHI_LOG_ERR;
		unsupp = " not supported";
	}

	char bufi[100], bufo[100];
	core->conf.log(core->conf.log_obj, level, NULL, t, "audio conversion%s: %s -> %s"
		, unsupp
		, phi_af_print(in, bufi, sizeof(bufi)), phi_af_print(out, bufo, sizeof(bufo)));
}

/*
This filter converts both format and sample rate.
Previous filter must deal with channel conversion.
*/
static int soxr_conv(void *ctx, phi_track *t)
{
	struct soxr *c = ctx;
	int val;
	struct phi_af inpcm, outpcm;

	switch (c->state) {
	case 0:
		inpcm = c->inpcm;
		outpcm = c->outpcm;

		// c->soxr.dither = 1;
		if (0 != (val = ffsoxr_create(&c->soxr, &inpcm, &outpcm))
			|| (core->conf.log_level >= PHI_LOG_DEBUG)) {
			log_pcmconv(val, &inpcm, &outpcm, t);
			if (val != 0) {
				t->error = PHI_E_ACONV;
				return PHI_ERR;
			}
		}

		c->state = 3;
		break;

	case 3:
		break;
	}

	if (t->chain_flags & PHI_FFWD) {
		c->soxr.in_i = t->data_in.ptr;
		c->soxr.inlen = t->data_in.len;
		c->soxr.inoff = 0;
	}
	c->soxr.fin = !!(t->chain_flags & PHI_FFIRST);
	if (ffsoxr_convert(&c->soxr, &t->data_out)) {
		errlog(t, "ffsoxr_convert(): %s", ffsoxr_errstr(&c->soxr));
		return PHI_ERR;
	}

	if (t->data_out.len == 0) {
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_MORE;
	}

	return PHI_DATA;
}

const phi_filter phi_soxr = {
	soxr_open, soxr_close, soxr_conv,
	"soxr-convert"
};
