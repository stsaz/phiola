/** phiola: FLAC decode
2018, Simon Zolin */

#include <acodec/alib3-bridge/flac.h>

struct flac_dec {
	ffflac_dec fl;
	struct phi_af fmt;
};

static void flac_dec_free(void *ctx, phi_track *t)
{
	struct flac_dec *f = ctx;
	ffflac_dec_close(&f->fl);
	ffmem_free(f);
}

static void* flac_dec_create(phi_track *t)
{
	int r;
	struct flac_dec *f = ffmem_new(struct flac_dec);

	struct flac_info info;
	info.minblock = t->audio.flac_minblock;
	info.maxblock = t->audio.flac_maxblock;
	info.bits = pcm_bits(t->audio.format.format);
	info.channels = t->audio.format.channels;
	info.sample_rate = t->audio.format.rate;
	f->fmt = t->audio.format;
	if (0 != (r = ffflac_dec_open(&f->fl, &info))) {
		errlog(t, "ffflac_dec_open(): %s", ffflac_dec_errstr(&f->fl));
		flac_dec_free(f, t);
		return PHI_OPEN_ERR;
	}

	t->data_type = "pcm";
	return f;
}

static int flac_dec_decode(void *ctx, phi_track *t)
{
	enum { I_HDR, I_DATA };
	struct flac_dec *f = ctx;
	int r;

	if (t->chain_flags & PHI_FFIRST) {

		return PHI_DONE;
	}

	if (t->audio.seek != -1) {
		ffflac_dec_seek(&f->fl, msec_to_samples(t->audio.seek, f->fmt.rate));
	}
	ffflac_dec_input(&f->fl, &t->data_in, t->audio.flac_samples, t->audio.pos);
	t->data_in.len = 0;

	r = ffflac_decode(&f->fl);
	switch (r) {
	case FFFLAC_RDATA:
		break;

	case FFFLAC_RWARN:
		warnlog(t, "ffflac_decode(): %s"
			, ffflac_dec_errstr(&f->fl));
		return PHI_MORE;
	}

	t->audio.pos = ffflac_dec_cursample(&f->fl);
	t->data_out.len = ffflac_dec_output(&f->fl, (void***)&t->data_out.ptr);
	dbglog(t, "decoded %L samples (%U)"
		, t->data_out.len / pcm_size1(&f->fmt), t->audio.pos);
	return PHI_OK;
}

const phi_filter phi_flac_dec = {
	flac_dec_create, flac_dec_free, flac_dec_decode,
	"flac-decode"
};
