/** phiola: WavPack input
2015, Simon Zolin */

#include <wavpack/wavpack-phi.h>

struct wvpk_dec {
	ffstr in;
	uint frame_size;
	wavpack_ctx *wp;
	uint frsize;
	int hdr_done;
	uint format;
	uint channels;

	union {
	short *pcm;
	int *pcm32;
	float *pcmf;
	};
	uint outcap; //samples
};

static void* wvpk_dec_create(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct wvpk_dec *w = phi_track_allocT(t, struct wvpk_dec);
	return w;
}

static void wvpk_dec_free(void *ctx, phi_track *t)
{
	struct wvpk_dec *w = ctx;
	if (w->wp)
		wavpack_decode_free(w->wp);
	ffmem_free(w->pcm);
	phi_track_free(t, w);
}

static int wvpk_dec_init(struct wvpk_dec *w, phi_track *t)
{
	if (!(w->wp = wavpack_decode_init())) {
		errlog(t, "wavpack_decode_init()");
		return PHI_ERR;
	}
	wavpack_info info = {};
	int r = wavpack_read_header(w->wp, w->in.ptr, w->in.len, &info);
	if (r < 0) {
		errlog(t, "wavpack_read_header()");
		return PHI_ERR;
	}

	w->format = t->audio.format.format;
	w->channels = t->audio.format.channels;
	w->frsize = pcm_size(w->format, w->channels);

	t->audio.decoder = "WavPack";
	t->audio.format.interleaved = 1;
	t->data_type = PHI_AC_PCM;
	w->frame_size = phi_af_size(&t->audio.format);
	return 0;
}

static int wvpk_dec_decode(void *ctx, phi_track *t)
{
	struct wvpk_dec *w = ctx;

	if (t->chain_flags & PHI_FFWD)
		w->in = t->data_in;

	if (w->in.len == 0)
		goto more;

	if (!w->hdr_done) {
		w->hdr_done = 1;
		if (wvpk_dec_init(w, t))
			return PHI_ERR;
	}

	if (6*4 > w->in.len) {
		errlog(t, "bad input data");
		return PHI_ERR;
	}

	uint blk_samples = ffint_le_cpu32_ptr(w->in.ptr + 5*4);

	if (w->outcap < blk_samples) {
		w->pcm = ffmem_realloc(w->pcm, blk_samples * sizeof(int) * w->channels);
		w->outcap = blk_samples;
	}

	int n = wavpack_decode(w->wp, w->in.ptr, w->in.len, w->pcm32, w->outcap);
	if (n <= 0) {
		if (n < 0)
			warnlog(t, "wavpack_decode(): %s", wavpack_errstr(w->wp));
		goto more;
	}

	FF_ASSERT((uint)n == blk_samples);

	switch (w->format) {
	case PHI_PCM_16:
		// in-place conversion: int[] -> short[]
		for (uint i = 0;  i < n * w->channels;  i++) {
			w->pcm[i] = (short)w->pcm32[i];
		}
		break;

	case PHI_PCM_32:
	case PHI_PCM_FLOAT32:
		break;

	default:
		FF_ASSERT(0);
		return PHI_ERR;
	}

	ffstr_set(&t->data_out, w->pcm, n * w->frsize);
	w->in.len = 0;
	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / w->frame_size, t->audio.pos);
	return PHI_DATA;

more:
	return !(t->chain_flags & PHI_FFIRST) ? PHI_MORE : PHI_DONE;
}

const phi_filter phi_wavpack_dec = {
	wvpk_dec_create, wvpk_dec_free, wvpk_dec_decode,
	"wavpack-decode"
};
