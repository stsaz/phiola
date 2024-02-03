/** phiola: FLAC decode
2018, Simon Zolin */

#include <acodec/alib3-bridge/flac-dec-if.h>

struct flac_dec {
	ffflac_dec fl;
	uint sample_size;
	uint64 last_page_pos;
};

static void flac_dec_free(void *ctx, phi_track *t)
{
	struct flac_dec *f = ctx;
	ffflac_dec_close(&f->fl);
	ffmem_free(f);
}

static void* flac_dec_create(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	int r;
	struct flac_dec *f = ffmem_new(struct flac_dec);

	flac_conf info = {
		.bps = phi_af_bits(&t->audio.format),
		.channels = t->audio.format.channels,
		.rate = t->audio.format.rate,

		.min_blocksize = t->audio.flac_minblock,
		.max_blocksize = t->audio.flac_maxblock,
	};
	if (0 != (r = ffflac_dec_open(&f->fl, &info))) {
		errlog(t, "ffflac_dec_open(): %s", ffflac_dec_errstr(&f->fl));
		flac_dec_free(f, t);
		return PHI_OPEN_ERR;
	}

	f->sample_size = phi_af_size(&t->audio.format);
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

	if (t->chain_flags & PHI_FFWD) {
		if (f->fl.frsample == ~0ULL || f->last_page_pos != t->audio.pos) {
			f->last_page_pos = t->audio.pos;
			f->fl.frsample = t->audio.pos;
		}

		ffflac_dec_input(&f->fl, t->data_in, t->audio.flac_samples);
		t->data_in.len = 0;
	}

	r = ffflac_decode(&f->fl, &t->data_out, &t->audio.pos);
	if (r < 0) {
		warnlog(t, "ffflac_decode(): %s"
			, ffflac_dec_errstr(&f->fl));
		return PHI_MORE;
	}

	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / f->sample_size, t->audio.pos);
	return PHI_OK;
}

const phi_filter phi_flac_dec = {
	flac_dec_create, flac_dec_free, flac_dec_decode,
	"flac-decode"
};
