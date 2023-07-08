/** phiola: .wav input/output
2015, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <avpack/wav-read.h>
#include <format/mmtag.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define verblog(t, ...)  phi_verblog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <format/wav-write.h>

struct wav_r {
	wavread wav;
	phi_track *trk;
	ffstr in;
	uint sample_rate;
	uint state;
};

static void wav_log(void *udata, const char *fmt, va_list va)
{
	struct wav_r *w = udata;
	phi_dbglogv(core, NULL, w->trk, fmt, va);
}

static void* wav_open(phi_track *t)
{
	struct wav_r *w = ffmem_new(struct wav_r);
	w->trk = t;
	wavread_open(&w->wav);
	w->wav.log = wav_log;
	w->wav.udata = w;
	return w;
}

static void wav_close(void *ctx, phi_track *t)
{
	struct wav_r *w = ctx;
	wavread_close(&w->wav);
	ffmem_free(w);
}

extern const phi_meta_if phi_metaif;
static void wav_meta(struct wav_r *w, phi_track *t)
{
	ffstr name, val;
	int tag = wavread_tag(&w->wav, &val);
	if (tag == 0)
		return;
	ffstr_setz(&name, ffmmtag_str[tag]);
	phi_metaif.set(&t->meta, name, val);
}

static int wav_process(void *ctx, phi_track *t)
{
	enum { I_HDR, I_DATA };
	struct wav_r *w = ctx;
	int r;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		w->in = t->data_in;
		t->data_in.len = 0;
	}

again:
	switch (w->state) {
	case I_HDR:
		break;

	case I_DATA:
		if (t->audio.seek_req && t->audio.seek != -1) {
			t->audio.seek_req = 0;
			wavread_seek(&w->wav, msec_to_samples(t->audio.seek, w->sample_rate));
		}
		break;
	}

	if (t->chain_flags & PHI_FFIRST)
		w->wav.fin = 1;

	for (;;) {
		r = wavread_process(&w->wav, &w->in, &t->data_out);
		switch (r) {
		case WAVREAD_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				if (!w->wav.inf_data)
					errlog(t, "file is incomplete");
				return PHI_DONE;
			}
			return PHI_MORE;

		case WAVREAD_DONE:
			return PHI_DONE;

		case WAVREAD_DATA:
			goto data;

		case WAVREAD_HEADER: {
			const struct wav_info *ai = wavread_info(&w->wav);
			t->audio.decoder = "WAVE";
			struct phi_af f = {
				.format = (ai->format == WAV_FLOAT) ? PHI_PCM_FLOAT32 : ai->format,
				.channels = ai->channels,
				.rate = ai->sample_rate,
				.interleaved = 1,
			};
			t->audio.format = f;
			t->audio.total = ai->total_samples;
			t->audio.bitrate = ai->bitrate;
			t->data_type = "pcm";

			w->sample_rate = ai->sample_rate;
			w->state = I_DATA;
			goto again;
		}

		case WAVREAD_TAG:
			wav_meta(w, t);
			break;

		case WAVREAD_SEEK:
			t->input.seek = wavread_offset(&w->wav);
			return PHI_MORE;

		case WAVREAD_ERROR:
		default:
			errlog(t, "wavread_decode(): %s", wavread_error(&w->wav));
			return PHI_ERR;
		}
	}

data:
	t->audio.pos = wavread_cursample(&w->wav);
	return PHI_DATA;
}

const struct phi_filter phi_wav_read = {
	wav_open, wav_close, wav_process,
	"wav-read"
};
