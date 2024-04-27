/** phiola: WavPack input
2015, Simon Zolin */

struct wvpk_dec {
	ffwvpack_dec wv;
	ffstr in;
	uint frame_size;
	uint outdata_delayed :1;
};

static void* wvpk_dec_create(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct wvpk_dec *w = phi_track_allocT(t, struct wvpk_dec);
	ffwvpk_dec_open(&w->wv);
	return w;
}

static void wvpk_dec_free(void *ctx, phi_track *t)
{
	struct wvpk_dec *w = ctx;
	ffwvpk_dec_close(&w->wv);
	phi_track_free(t, w);
}

/** Return bits/sec. */
#define pcm_brate(bytes, samples, rate) \
	FFINT_DIVSAFE((uint64)(bytes) * 8 * (rate), samples)

static void wv_info(struct wvpk_dec *w, phi_track *t, const struct ffwvpk_info *info)
{
	dbglog(t, "lossless:%u  compression:%u  MD5:%16xb"
		, (int)info->lossless
		, info->comp_level
		, info->md5);
	t->audio.decoder = "WavPack";
	struct phi_af f = {
		.format = info->format,
		.channels = info->channels,
		.rate = info->sample_rate,
		.interleaved = 1,
	};
	t->audio.format = f;
	t->audio.bitrate = pcm_brate(t->input.size, t->audio.total, info->sample_rate);
	t->data_type = "pcm";
}

static int wvpk_dec_decode(void *ctx, phi_track *t)
{
	struct wvpk_dec *w = ctx;

	if (w->outdata_delayed) {
		w->outdata_delayed = 0;
		if (t->audio.seek > 0) {
			return PHI_MORE;
		}
	}

	if (t->chain_flags & PHI_FFWD) {
		w->in = t->data_in;
	}

	int r = ffwvpk_decode(&w->wv, &w->in, &t->data_out);

	switch (r) {
	case FFWVPK_RHDR:
		wv_info(w, t, ffwvpk_dec_info(&w->wv));
		w->frame_size = phi_af_size(&t->audio.format);
		w->outdata_delayed = 1;
		t->data_out.len = 0;
		return PHI_DATA;

	case FFWVPK_RDATA:
		break;

	case FFWVPK_RMORE:
		if (t->chain_flags & PHI_FFIRST) {
			return PHI_DONE;
		}
		return PHI_MORE;

	case FFWVPK_RERR:
		errlog(t, "ffwvpk_decode(): %s", ffwvpk_dec_error(&w->wv));
		return PHI_ERR;

	default:
		FF_ASSERT(0);
		return PHI_ERR;
	}

	dbglog(t, "decoded %L samples @%U"
		, (size_t)t->data_out.len / w->frame_size, t->audio.pos);
	return PHI_DATA;
}

const phi_filter phi_wavpack_dec = {
	wvpk_dec_create, wvpk_dec_free, wvpk_dec_decode,
	"wavpack-decode"
};
