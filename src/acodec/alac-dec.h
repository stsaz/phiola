/** phiola: ALAC input
2016, Simon Zolin */

#include <ffbase/vector.h>
#include <avpack/base/alac.h>
#include <ALAC/ALAC-phi.h>

typedef struct alac_in {
	struct alac_ctx *al;
	struct phi_af fmt;
	uint bitrate; // 0 if unknown
	ffstr in;
	ffvec buf;
} alac_in;

static void* alac_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	alac_in *a = phi_track_allocT(t, alac_in);

	if (!(a->al = alac_init(t->data_in.ptr, t->data_in.len))) {
		errlog(t, "alac_init(): bad 'magic cookie'");
		phi_track_free(t, a);
		return PHI_OPEN_ERR;
	}
	t->data_in.len = 0;

	const struct alac_conf *conf = (void*)t->data_in.ptr;
	a->fmt.format = conf->bit_depth;
	a->fmt.channels = conf->channels;
	a->fmt.rate = ffint_be_cpu32_ptr(conf->sample_rate);
	a->bitrate = ffint_be_cpu32_ptr(conf->avg_bitrate);

	uint n = ffint_be_cpu32_ptr(conf->frame_length) * phi_af_size(&a->fmt);
	ffvec_alloc(&a->buf, n, 1);

	t->audio.end_padding = (t->audio.total != ~0ULL);
	if (a->bitrate != 0)
		t->audio.bitrate = a->bitrate;
	t->audio.format = a->fmt;
	t->audio.format.interleaved = 1;
	t->audio.decoder = "ALAC";
	t->data_type = PHI_AC_PCM;
	return a;
}

static void alac_close(void *ctx, phi_track *t)
{
	alac_in *a = ctx;
	alac_free(a->al);
	ffvec_free(&a->buf);
	phi_track_free(t, a);
}

static int alac_in_decode(void *ctx, phi_track *t)
{
	alac_in *a = ctx;

	if (t->chain_flags & PHI_FFWD)
		a->in = t->data_in;

	int r = alac_decode(a->al, a->in.ptr, a->in.len, a->buf.ptr);
	if (r <= 0) {
		if (r < 0)
			warnlog(t, "alac_decode(): %d", r);
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_MORE;
	}

	a->in.len = 0;
	ffstr_set(&t->data_out, a->buf.ptr, r * phi_af_size(&a->fmt));
	dbglog(t, "decoded %u samples @%U"
		, t->data_out.len / phi_af_size(&a->fmt), t->audio.pos);
	return PHI_DATA;
}

const phi_filter phi_alac_dec = {
	alac_open, alac_close, alac_in_decode,
	"alac-decode"
};
