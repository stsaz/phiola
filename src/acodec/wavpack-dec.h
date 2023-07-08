/** phiola: WavPack input
2015, Simon Zolin */

struct wvpk_dec {
	ffwvpack_dec wv;
	ffstr in;
	uint frsize;
	uint sample_rate;
	uint outdata_delayed :1;
};

static void* wvpk_dec_create(phi_track *t)
{
	struct wvpk_dec *w = ffmem_new(struct wvpk_dec);
	ffwvpk_dec_open(&w->wv);
	return w;
}

static void wvpk_dec_free(void *ctx, phi_track *t)
{
	struct wvpk_dec *w = ctx;
	ffwvpk_dec_close(&w->wv);
	ffmem_free(w);
}

/** Return bits/sec. */
#define pcm_brate(bytes, samples, rate) \
	FFINT_DIVSAFE((uint64)(bytes) * 8 * (rate), samples)

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

	if ((t->chain_flags & PHI_FFWD) && t->audio.seek != -1) {
		ffwvpk_dec_seek(&w->wv, msec_to_samples(t->audio.seek, w->sample_rate));
	}

	int r = ffwvpk_decode(&w->wv, &w->in, &t->data_out, t->audio.pos);

	switch (r) {
	case FFWVPK_RHDR: {
		const struct ffwvpk_info *info = ffwvpk_dec_info(&w->wv);
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
		w->sample_rate = info->sample_rate;
		t->audio.bitrate = pcm_brate(t->input.size, t->audio.total, info->sample_rate);
		t->data_type = "pcm";
		w->frsize = pcm_size(info->format, info->channels);
		w->outdata_delayed = 1;
		t->data_out.len = 0;
		return PHI_DATA;
	}

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

	dbglog(t, "decoded %L samples (%U)"
		, (size_t)t->data_out.len / w->frsize, (int64)w->wv.samp_idx);
	t->audio.pos = w->wv.samp_idx;
	return PHI_DATA;
}

const phi_filter phi_wavpack_dec = {
	wvpk_dec_create, wvpk_dec_free, wvpk_dec_decode,
	"wavpack-decode"
};
