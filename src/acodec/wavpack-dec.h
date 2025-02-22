/** phiola: WavPack input
2015, Simon Zolin */

struct wvpk_dec {
	ffwvpack_dec wv;
	ffstr in;
	uint frame_size;
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
	t->data_type = "pcm";
}

static int wvpk_dec_decode(void *ctx, phi_track *t)
{
	struct wvpk_dec *w = ctx;

	if (t->chain_flags & PHI_FFWD) {
		w->in = t->data_in;
	}

again:
	int r = ffwvpk_decode(&w->wv, &w->in, &t->data_out);

	switch (r) {
	case FFWVPK_RHDR:
		wv_info(w, t, ffwvpk_dec_info(&w->wv));
		w->frame_size = phi_af_size(&t->audio.format);
		goto again;

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
