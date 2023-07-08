/** phiola: ALAC input
2016, Simon Zolin */

#include <acodec/alib3-bridge/alac.h>

typedef struct alac_in {
	ffalac alac;
} alac_in;

static void* alac_open(phi_track *t)
{
	alac_in *a = ffmem_new(alac_in);

	if (0 != ffalac_open(&a->alac, t->data_in.ptr, t->data_in.len)) {
		errlog(t, "ffalac_open(): %s", ffalac_errstr(&a->alac));
		ffmem_free(a);
		return PHI_OPEN_ERR;
	}
	a->alac.total_samples = t->audio.total;
	t->data_in.len = 0;

	if (a->alac.bitrate != 0)
		t->audio.bitrate = a->alac.bitrate;
	t->audio.format = a->alac.fmt;
	t->audio.format.interleaved = 1;
	t->audio.decoder = "ALAC";
	t->data_type = "pcm";
	return a;
}

static void alac_close(void *ctx, phi_track *t)
{
	alac_in *a = ctx;
	ffalac_close(&a->alac);
	ffmem_free(a);
}

static int alac_in_decode(void *ctx, phi_track *t)
{
	alac_in *a = ctx;

	if (t->chain_flags & PHI_FFWD) {
		a->alac.data = t->data_in.ptr,  a->alac.datalen = t->data_in.len;
		t->data_in.len = 0;
		a->alac.cursample = t->audio.pos;
		if (t->audio.seek != -1) {
			uint64 seek = msec_to_samples(t->audio.seek, a->alac.fmt.rate);
			ffalac_seek(&a->alac, seek);
		}
	}

	int r;
	r = ffalac_decode(&a->alac);
	if (r == FFALAC_RERR) {
		errlog(t, "ffalac_decode(): %s", ffalac_errstr(&a->alac));
		return PHI_ERR;

	} else if (r == FFALAC_RMORE) {
		if (t->chain_flags & PHI_FFIRST) {
			return PHI_DONE;
		}
		return PHI_MORE;
	}

	dbglog(t, "decoded %u samples (%U)"
		, a->alac.pcmlen / pcm_size1(&a->alac.fmt), ffalac_cursample(&a->alac));
	t->audio.pos = ffalac_cursample(&a->alac);

	ffstr_set(&t->data_out, a->alac.pcm, a->alac.pcmlen);
	return PHI_DATA;
}

const phi_filter phi_alac_dec = {
	alac_open, alac_close, alac_in_decode,
	"alac-decode"
};
