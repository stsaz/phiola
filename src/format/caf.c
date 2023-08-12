/** phiola: CAF input.
2020, Simon Zolin */

#include <track.h>
#include <avpack/caf-read.h>

extern const phi_core *core;
extern const phi_meta_if phi_metaif;
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)

struct caf_r {
	cafread caf;
	void *trk;
	ffstr in;
	uint state;
};

void caf_log(void *udata, const char *fmt, va_list va)
{
	struct caf_r *c = udata;
	phi_dbglogv(core, NULL, c->trk, fmt, va);
}

static void* caf_open(phi_track *t)
{
	struct caf_r *c = ffmem_new(struct caf_r);
	c->trk = t;
	cafread_open(&c->caf);
	c->caf.log = caf_log;
	c->caf.udata = c;
	return c;
}

static void caf_close(void *ctx, phi_track *t)
{
	struct caf_r *c = ctx;
	cafread_close(&c->caf);
	ffmem_free(c);
}

static const ffbyte caf_codecs[] = {
	CAF_AAC, CAF_ALAC, CAF_LPCM,
};
static const char* const caf_codecs_str[] = {
	"aac.decode", "alac.decode", "",
};

static int caf_process(void *ctx, phi_track *t)
{
	enum { I_HDR, I_DATA };
	struct caf_r *c = ctx;
	int r;

	if (t->chain_flags & PHI_FSTOP) {
		t->data_out.len = 0;
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		c->in = t->data_in;
		t->data_in.len = 0;
	}

	switch (c->state) {
	case I_HDR:
		break;

	case I_DATA:
		break;
	}

	for (;;) {
		r = cafread_process(&c->caf, &c->in, &t->data_out);
		switch (r) {
		case CAFREAD_MORE_OR_DONE:
			if (t->chain_flags & PHI_FFIRST) {
				t->data_out.len = 0;
				return PHI_DONE;
			}
			// fallthrough

		case CAFREAD_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				errlog(t, "file is incomplete");
				t->data_out.len = 0;
				return PHI_DONE;
			}
			return PHI_MORE;

		case CAFREAD_DONE:
			t->data_out.len = 0;
			return PHI_DONE;

		case CAFREAD_DATA:
			goto data;

		case CAFREAD_HEADER: {
			const caf_info *ai = cafread_info(&c->caf);
			dbglog(t, "codec:%u  conf:%*xb  packets:%U  frames/packet:%u  bytes/packet:%u"
				, ai->codec, ai->codec_conf.len, ai->codec_conf.ptr
				, ai->total_packets, ai->packet_frames, ai->packet_bytes);

			int i = ffarrint8_find(caf_codecs, FF_COUNT(caf_codecs), ai->codec);
			if (i == -1) {
				errlog(t, "unsupported codec: %xu", ai->codec);
				return PHI_ERR;
			}

			const char *codec = caf_codecs_str[i];
			if (codec[0] == '\0') {
				if (ai->format & CAF_FMT_FLOAT) {
					errlog(t, "float data isn't supported");
					return PHI_ERR;
				}
				if (!(ai->format & CAF_FMT_LE)) {
					errlog(t, "big-endian data isn't supported");
					return PHI_ERR;
				}
				t->data_type = "pcm";
				t->audio.format.format = ai->format & 0xff;
				t->audio.format.interleaved = 1;
			} else if (!core->track->filter(t, core->mod(codec), 0)) {
				return PHI_ERR;
			}
			t->audio.format.channels = ai->channels;
			t->audio.format.rate = ai->sample_rate;
			t->audio.total = ai->total_frames;
			t->audio.bitrate = ai->bitrate;

			if (t->conf.info_only)
				return PHI_LASTOUT;

			t->data_out = ai->codec_conf;
			c->state = I_DATA;
			return PHI_DATA;
		}

		case CAFREAD_TAG: {
			ffstr name, val;
			name = cafread_tag(&c->caf, &val);
			phi_metaif.set(&t->meta, name, val, 0);
			break;
		}

		case CAFREAD_SEEK:
			t->input.seek = cafread_offset(&c->caf);
			return PHI_MORE;

		case CAFREAD_ERROR:
		default:
			errlog(t, "cafread_process(): %s", cafread_error(&c->caf));
			return PHI_ERR;
		}
	}

data:
	t->audio.pos = cafread_cursample(&c->caf);
	dbglog(c->trk, "packet size:%L @%U", t->data_out.len, t->audio.pos);
	return PHI_DATA;
}

const phi_filter phi_caf_read = {
	caf_open, (void*)caf_close, (void*)caf_process,
	"caf-read"
};
