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

static inline char* af_print(const struct phi_af *af, char *buf, ffsize cap)
{
	int r = ffs_format_r0(buf, cap - 1, "%s/%u/%u/%s"
		, phi_af_name(af->format), af->rate, af->channels, (af->interleaved) ? "i" : "ni");
	buf[r] = '\0';
	return buf;
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
		, af_print(in, bufi, sizeof(bufi)), af_print(out, bufo, sizeof(bufo)));
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
			if (val != 0)
				return PHI_ERR;
		}

		c->state = 3;
		break;

	case 3:
		break;
	}

	c->soxr.in_i = t->data_in.ptr;
	c->soxr.inlen = t->data_in.len;
	if (t->chain_flags & PHI_FFIRST)
		c->soxr.fin = 1;
	if (0 != ffsoxr_convert(&c->soxr)) {
		errlog(t, "ffsoxr_convert(): %s", ffsoxr_errstr(&c->soxr));
		return PHI_ERR;
	}

	ffstr_set(&t->data_out, c->soxr.out, c->soxr.outlen);

	if (c->soxr.outlen == 0) {
		if (t->chain_flags & PHI_FFIRST)
			return PHI_DONE;
	}

	ffstr_set(&t->data_in, c->soxr.in_i, c->soxr.inlen);
	return PHI_OK;
}

const phi_filter phi_soxr = {
	soxr_open, soxr_close, soxr_conv,
	"soxr-convert"
};
