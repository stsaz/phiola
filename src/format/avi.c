/** phiola: AVI input.
2016, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <avpack/avi-read.h>
#include <format/mmtag.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct avi_r {
	aviread avi;
	void *trk;
	ffstr in;
	uint state;
};

void avi_log(void *udata, const char *fmt, va_list va)
{
	struct avi_r *a = udata;
	phi_dbglogv(core, NULL, a->trk, fmt, va);
}

static void* avi_open(phi_track *t)
{
	struct avi_r *a = phi_track_allocT(t, struct avi_r);
	a->trk = t;
	aviread_open(&a->avi);
	a->avi.log = avi_log;
	a->avi.udata = a;
	return a;
}

static void avi_close(void *ctx, phi_track *t)
{
	struct avi_r *a = ctx;
	aviread_close(&a->avi);
	phi_track_free(t, a);
}

extern const phi_meta_if phi_metaif;
static void avi_meta(struct avi_r *a, phi_track *t)
{
	ffstr name, val;
	int tag = aviread_tag(&a->avi, &val);
	if (tag == -1)
		return;
	ffstr_setz(&name, ffmmtag_str[tag]);
	phi_metaif.set(&t->meta, name, val, 0);
}

static const ushort avi_codecs[] = {
	AVI_A_AAC, AVI_A_MP3,
};
static const char* const avi_codecs_str[] = {
	"ac-aac.decode", "ac-mpeg.decode",
};

static const struct avi_audio_info* get_first_audio_track(struct avi_r *a)
{
	for (ffuint i = 0;  ;  i++) {
		const struct avi_audio_info *ai = aviread_track_info(&a->avi, i);
		if (ai == NULL)
			break;

		if (ai->type == 1) {
			aviread_track_activate(&a->avi, i);
			return ai;
		}
	}
	return NULL;
}

static int avi_process(void *ctx, phi_track *t)
{
	enum { I_HDR, I_DATA };
	struct avi_r *a = ctx;
	int r;

	if (t->chain_flags & PHI_FSTOP) {

		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		a->in = t->data_in;
		t->data_in.len = 0;
	}

	switch (a->state) {
	case I_HDR:
		break;

	case I_DATA:
		break;
	}

	for (;;) {
		r = aviread_process(&a->avi, &a->in, &t->data_out);
		switch (r) {
		case AVIREAD_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				errlog(t, "file is incomplete");

				return PHI_DONE;
			}
			return PHI_MORE;

		case AVIREAD_DONE:
			t->data_out.len = 0;
			return PHI_DONE;

		case AVIREAD_DATA:
			goto data;

		case AVIREAD_HEADER: {
			const struct avi_audio_info *ai = get_first_audio_track(a);
			dbglog(t, "codec:%u  conf:%*xb"
				, ai->codec, ai->codec_conf.len, ai->codec_conf.ptr);
			if (ai->codec == AVI_A_PCM) {
				t->audio.format.format = ai->bits;
				t->audio.format.interleaved = 1;
				t->data_type = "pcm";
			} else {
				int i = ffarrint16_find(avi_codecs, FF_COUNT(avi_codecs), ai->codec);
				if (i == -1) {
					errlog(t, "unsupported codec: %xu", ai->codec);
					return PHI_ERR;
				}

				const char *codec = avi_codecs_str[i];
				if (!core->track->filter(t, core->mod(codec), 0)) {
					return PHI_ERR;
				}
			}
			t->audio.format.channels = ai->channels;
			t->audio.format.rate = ai->sample_rate;
			t->audio.total = msec_to_samples(ai->duration_msec, ai->sample_rate);
			t->audio.bitrate = ai->bitrate;

			t->data_out = ai->codec_conf;
			a->state = I_DATA;
			return PHI_DATA;
		}

		case AVIREAD_TAG:
			avi_meta(a, t);
			break;

		// case AVIREAD_SEEK:
			// t->input.seek = aviread_offset(&a->avi);
			// return PHI_MORE;

		case AVIREAD_ERROR:
		default:
			errlog(t, "aviread_process(): %s", aviread_error(&a->avi));
			return PHI_ERR;
		}
	}

data:
	t->audio.pos = aviread_cursample(&a->avi);
	dbglog(a->trk, "frame size:%L @%U", t->data_out.len, t->audio.pos);
	return PHI_DATA;
}

const phi_filter phi_avi_read = {
	avi_open, (void*)avi_close, (void*)avi_process,
	"avi-read"
};
