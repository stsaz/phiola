/** phiola: MKV input.
2016, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <avpack/mkv-read.h>
#include <format/mmtag.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct mkv_r {
	mkvread mkv;
	ffstr in;
	ffstr vorb_in;
	struct mkv_vorbis mkv_vorbis;
	void *trk;
	uint64 atrack;
	uint state;
	uint sample_rate;
};

static void mkv_log(void *udata, const char *fmt, va_list va)
{
	struct mkv_r *m = udata;
	phi_dbglogv(core, NULL, m->trk, fmt, va);
}

static void* mkv_open(phi_track *t)
{
	struct mkv_r *m = ffmem_new(struct mkv_r);
	ffuint64 total_size = 0;
	if (t->input.size != ~0ULL)
		total_size = t->input.size;
	mkvread_open(&m->mkv, total_size);
	m->mkv.log = mkv_log;
	m->mkv.udata = m;
	m->trk = t;
	return m;
}

static void mkv_close(void *ctx, phi_track *t)
{
	struct mkv_r *m = ctx;
	mkvread_close(&m->mkv);
	ffmem_free(m);
}

extern const phi_meta_if phi_metaif;
static void mkv_meta(struct mkv_r *m, phi_track *t)
{
	ffstr name, val;
	name = mkvread_tag(&m->mkv, &val);
	phi_metaif.set(&t->meta, name, val, 0);
}

static const ushort mkv_codecs[] = {
	MKV_A_AAC, MKV_A_ALAC, MKV_A_MPEGL3, MKV_A_OPUS, MKV_A_VORBIS, MKV_A_PCM,
};
static const char* const mkv_codecs_str[] = {
	"aac.decode", "alac.decode", "mpeg.decode", "opus.decode", "vorbis.decode", "",
};
static const ushort mkv_vcodecs[] = {
	MKV_V_AVC, MKV_V_HEVC,
};
static const char* const mkv_vcodecs_str[] = {
	"H.264", "H.265",
};

static void print_tracks(struct mkv_r *m, phi_track *t)
{
	for (ffuint i = 0;  ;  i++) {
		const struct mkvread_audio_info *ai = mkvread_track_info(&m->mkv, i);
		if (ai == NULL)
			break;

		switch (ai->type) {
		case MKV_TRK_VIDEO: {
			const struct mkvread_video_info *vi = mkvread_track_info(&m->mkv, i);
			int i = ffarrint16_find(mkv_vcodecs, FF_COUNT(mkv_vcodecs), vi->codec);
			if (i != -1)
				t->video.decoder = mkv_vcodecs_str[i];

			dbglog(t, "track#%u: codec:%s  size:%ux%u"
				, i
				, t->video.decoder, vi->width, vi->height);
			t->video.width = vi->width;
			t->video.height = vi->height;
			break;
		}
		case MKV_TRK_AUDIO:
			dbglog(t, "track#%u: codec:%d  magic:%*xb  duration:%U  format:%u/%u"
				, i
				, ai->codec
				, ai->codec_conf.len, ai->codec_conf.ptr
				, ai->duration_msec
				, ai->sample_rate, ai->channels);
			break;
		}
	}
}

static const struct mkvread_audio_info* get_first_audio_track(struct mkv_r *m)
{
	for (ffuint i = 0;  ;  i++) {
		const struct mkvread_audio_info *ai = mkvread_track_info(&m->mkv, i);
		if (ai == NULL)
			break;

		if (ai->type == MKV_TRK_AUDIO) {
			m->atrack = ai->id;
			return ai;
		}
	}
	return NULL;
}

static int mkv_process(void *ctx, phi_track *t)
{
	enum { I_HDR, I_VORBIS_HDR, I_DATA, };
	struct mkv_r *m = ctx;
	int r;

	if (t->chain_flags & PHI_FSTOP) {
		t->data_out.len = 0;
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		m->in = t->data_in;
		t->data_in.len = 0;
	}

again:
	switch (m->state) {
	case I_HDR:
		break;

	case I_VORBIS_HDR:
		r = mkv_vorbis_hdr(&m->mkv_vorbis, &m->vorb_in, &t->data_out);
		if (r < 0) {
			errlog(t, "mkv_vorbis_hdr()");
			return PHI_ERR;
		} else if (r == 1) {
			m->state = I_DATA;
		} else {
			return PHI_DATA;
		}
		break;

	case I_DATA:
		if (t->audio.seek_req && t->audio.seek != -1) {
			t->audio.seek_req = 0;
			mkvread_seek(&m->mkv, t->audio.seek);
			dbglog(t, "seek: %Ums", t->audio.seek);
		}
		break;
	}

	ffstr out;
	for (;;) {
		r = mkvread_process(&m->mkv, &m->in, &out);
		switch (r) {
		case MKVREAD_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				dbglog(t, "file is incomplete");
				return PHI_DONE;
			}
			return PHI_MORE;

		case MKVREAD_DONE:
			return PHI_DONE;

		case MKVREAD_DATA:
			if (mkvread_block_trackid(&m->mkv) != m->atrack)
				continue;
			goto data;

		case MKVREAD_HEADER: {
			print_tracks(m, t);
			const struct mkvread_audio_info *ai = get_first_audio_track(m);
			if (ai == NULL) {
				errlog(t, "no audio track found");
				return PHI_ERR;
			}

			int i = ffarrint16_find(mkv_codecs, FF_COUNT(mkv_codecs), ai->codec);
			if (i == -1) {
				errlog(t, "unsupported codec: %xu", ai->codec);
				return PHI_ERR;
			}

			const char *codec = mkv_codecs_str[i];
			if (codec[0] == '\0') {
				t->data_type = "pcm";
				t->audio.format.format = ai->bits;
				t->audio.format.interleaved = 1;

			} else if (!t->conf.stream_copy) {
				if (!core->track->filter(t, core->mod(codec), 0)) {
					return PHI_ERR;
				}
				if (ai->codec == MKV_A_VORBIS
					&& !core->track->filter(t, core->mod("format.vorbismeta"), 0))
					return PHI_ERR;
			}
			t->audio.format.channels = ai->channels;
			t->audio.format.rate = ai->sample_rate;
			m->sample_rate = ai->sample_rate;
			t->audio.total = msec_to_samples(ai->duration_msec, ai->sample_rate);
			if (t->input.size != ~0ULL && ai->duration_msec != 0)
				t->audio.bitrate = (t->input.size * 8 * 1000) / ai->duration_msec;

			switch (ai->codec) {
			case MKV_A_AAC:
				t->data_type = "aac";
				t->data_out = ai->codec_conf;
				break;

			case MKV_A_VORBIS:
				t->data_type = "Vorbis";
				ffstr_set2(&m->vorb_in, &ai->codec_conf);
				m->state = I_VORBIS_HDR;
				goto again;

			case MKV_A_MPEGL3:
				t->data_type = "mpeg";
				break;

			case MKV_A_OPUS:
				if (t->conf.stream_copy) {
					t->data_type = "Opus";
					t->oaudio.ogg_gen_opus_tag = 1;
				}
				t->data_out = ai->codec_conf;
				break;

			default:
				t->data_out = ai->codec_conf;
			}

			m->state = I_DATA;
			return PHI_DATA;
		}

		case MKVREAD_TAG:
			mkv_meta(m, t);
			break;

		case MKVREAD_SEEK:
			t->input.seek = mkvread_offset(&m->mkv);
			return PHI_MORE;

		case MKVREAD_ERROR:
		default:
			errlog(t, "mkvread_read(): %s  offset:%xU"
				, mkvread_error(&m->mkv), mkvread_offset(&m->mkv));
			return PHI_ERR;
		}
	}

data:
	t->audio.pos = msec_to_samples(mkvread_curpos(&m->mkv), m->sample_rate);
	dbglog(t, "data size:%L  pos:%U", out.len, t->audio.pos);
	t->data_out = out;
	return PHI_DATA;
}

const phi_filter phi_mkv_read = {
	mkv_open, (void*)mkv_close, (void*)mkv_process,
	"mkv-read"
};
