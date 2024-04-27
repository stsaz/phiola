/** phiola: APE input
2015, Simon Zolin */

struct ape_dec {
	ffape ap;
	uint state;
};

static void* ape_dec_create(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct ape_dec *a = phi_track_allocT(t, struct ape_dec);
	ffape_open(&a->ap);
	return a;
}

static void ape_dec_free(void *ctx, phi_track *t)
{
	struct ape_dec *a = ctx;
	ffape_close(&a->ap);
	phi_track_free(t, a);
}

/** Return bits/sec. */
#define pcm_brate(bytes, samples, rate) \
	FFINT_DIVSAFE((uint64)(bytes) * 8 * (rate), samples)

static void ape_info(struct ape_dec *a, phi_track *t, const ffape_info *info)
{
	t->audio.decoder = "APE";
	struct phi_af f = {
		.format = info->fmt.format,
		.channels = info->fmt.channels,
		.rate = info->fmt.rate,
		.interleaved = 1,
	};
	t->audio.format = f;
	t->data_type = "pcm";
	t->audio.bitrate = pcm_brate(t->input.size, t->audio.total, info->fmt.rate);
	t->audio.total = info->total_samples;
}

static int ape_dec_decode(void *ctx, phi_track *t)
{
	struct ape_dec *a = ctx;
	int r = ffape_decode(&a->ap, &t->data_in, &t->data_out, t->audio.ape_block_samples, t->audio.ape_align4);
	switch (r) {
	case FFAPE_RMORE:
		if (t->chain_flags & PHI_FFIRST) {
			return PHI_DONE;
		}
		return PHI_MORE;

	case FFAPE_RHDR:
		ape_info(a, t, &a->ap.info);
		return PHI_DATA;

	case FFAPE_RDATA:
		goto data;

	case FFAPE_RERR:
		errlog(t, "ffape_decode(): %s", ffape_errstr(&a->ap));
		return PHI_ERR;

	default:
		FF_ASSERT(0);
		return PHI_ERR;
	}

data:
	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / phi_af_size(&a->ap.info.fmt), t->audio.pos);
	return PHI_DATA;
}

static const phi_filter phi_ape_dec = {
	ape_dec_create, ape_dec_free, ape_dec_decode,
	"ape-decode"
};
