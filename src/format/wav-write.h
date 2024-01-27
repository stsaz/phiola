/** phiola: .wav write
2015,2021, Simon Zolin */

#include <avpack/wav-write.h>

struct wav_w {
	wavwrite wav;
	ffstr in;
	uint state;
	uint samp_size;
	uint64 total;
};

static void* wavw_open(phi_track *t)
{
	if (!ffsz_eq(t->data_type, "pcm")) {
		errlog(t, "unsupported input data format: %s", t->data_type);
		return PHI_OPEN_ERR;
	}

	struct wav_w *w = ffmem_new(struct wav_w);
	return w;
}

static void wavw_close(struct wav_w *w, phi_track *t)
{
	wavwrite_close(&w->wav);
	ffmem_free(w);
}

/** Get size of 1 sample (in bytes). */
#define pcm_size(format, channels)  (pcm_bits(format) / 8 * (channels))
#define pcm_size1(f)  pcm_size((f)->format, (f)->channels)

static int wavw_process(struct wav_w *w, phi_track *t)
{
	int r;

	switch (w->state) {
	case 0:
		w->state = 1;
		if (!t->oaudio.format.interleaved) {
			t->oaudio.conv_format.interleaved = 1;
			return PHI_MORE;
		}
		// fallthrough

	case 1: {
		if (!t->oaudio.format.interleaved
			|| t->oaudio.format.format == PHI_PCM_8) {
			errlog(t, "audio format is not supported");
			return PHI_ERR;
		}

		const struct phi_af *of = &t->oaudio.format;
		struct wav_info info = {
			.format = (of->format == PHI_PCM_FLOAT32) ? WAV_FLOAT : of->format,
			.sample_rate = of->rate,
			.channels = of->channels,
		};
		if (t->audio.total != ~0ULL && t->audio.total != 0)
			info.total_samples = ((t->audio.total - t->audio.pos) * of->rate / t->audio.format.rate);
		wavwrite_create(&w->wav, &info);
		w->samp_size = pcm_size1(of);
		w->state = 2;
	}
	}

	if (t->chain_flags & PHI_FFWD) {
		w->in = t->data_in;
		if (t->chain_flags & PHI_FFIRST)
			wavwrite_finish(&w->wav);
	}

	for (;;) {
		r = wavwrite_process(&w->wav, &w->in, &t->data_out);
		switch (r) {
		case WAVWRITE_HEADER:
			goto data;

		case WAVWRITE_SEEK:
			if (t->output.cant_seek) {
				warnlog(t, "can't seek to finalize WAV header");
				return PHI_DONE;
			}
			t->output.seek = wavwrite_offset(&w->wav);
			continue;

		case WAVWRITE_DATA:
			w->total += t->data_out.len;
			goto data;

		case WAVWRITE_MORE:
			return PHI_MORE;

		case WAVWRITE_DONE:
			verblog(t, "total samples: %,U", w->total / w->samp_size);
			return PHI_DONE;

		case WAVWRITE_ERROR:
			errlog(t, "wavwrite_process(): %s", wavwrite_error(&w->wav));
			return PHI_ERR;
		}
	}

data:
	dbglog(t, "output: %L bytes", t->data_out.len);
	return PHI_DATA;
}

const struct phi_filter phi_wav_write = {
	wavw_open, (void*)wavw_close, (void*)wavw_process,
	"wav-write"
};
