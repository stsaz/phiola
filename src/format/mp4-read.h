/** phiola: .mp4 read
2021, Simon Zolin */

#include <avpack/mp4-read.h>

struct mp4_r {
	mp4read mp;
	ffstr in;
	void *trk;
	uint rate;
	uint state;
};

static void mp4_log(void *udata, const char *fmt, va_list va)
{
	struct mp4_r *m = udata;
	phi_dbglogv(core, NULL, m->trk, fmt, va);
}

static void* mp4r_create(phi_track *t)
{
	struct mp4_r *m = phi_track_allocT(t, struct mp4_r);
	m->trk = t;

	mp4read_open(&m->mp);
	m->mp.log = mp4_log;
	m->mp.udata = m;

	if (t->input.size != ~0ULL)
		m->mp.total_size = t->input.size;

	t->data_type = "mp4";
	return m;
}

static void mp4r_free(struct mp4_r *m, phi_track *t)
{
	mp4read_close(&m->mp);
	phi_track_free(t, m);
}

static void mp4_meta(struct mp4_r *m, phi_track *t)
{
	ffstr name, val;
	int tag = mp4read_tag(&m->mp, &val);
	if (tag == 0)
		return;
	ffstr_setz(&name, ffmmtag_str[tag]);

	dbglog(t, "tag: %S: %S", &name, &val);

	core->metaif->set(&t->meta, name, val, 0);
}

static void print_tracks(struct mp4_r *m, phi_track *t)
{
	for (uint i = 0;  ;  i++) {
		const struct mp4read_audio_info *ai = mp4read_track_info(&m->mp, i);
		if (ai == NULL)
			break;

		switch (ai->type) {
		case 0: {
			const struct mp4read_video_info *vi = mp4read_track_info(&m->mp, i);
			dbglog(t, "track#%u: codec:%s  size:%ux%u"
				, i
				, vi->codec_name, vi->width, vi->height);
			t->video.decoder = vi->codec_name;
			t->video.width = vi->width;
			t->video.height = vi->height;
			break;
		}
		case 1:
			dbglog(t, "track#%u: codec:%s  magic:%*xb  total_samples:%u  format:%u/%u"
				, i
				, ai->codec_name
				, ai->codec_conf.len, ai->codec_conf.ptr
				, ai->total_samples
				, ai->format.rate, ai->format.channels);
			break;
		}
	}
}

static const struct mp4read_audio_info* get_first_audio_track(struct mp4_r *m)
{
	for (ffuint i = 0;  ;  i++) {
		const struct mp4read_audio_info *ai = mp4read_track_info(&m->mp, i);
		if (ai == NULL)
			break;
		if (ai->type == 1) {
			mp4read_track_activate(&m->mp, i);
			return ai;
		}
	}
	return NULL;
}

static const char* mp4r_info(struct mp4_r *m, phi_track *t, const struct mp4read_audio_info *ai)
{
	struct phi_af f = {
		.format = ai->format.bits,
		.rate = ai->format.rate,
		.channels = ai->format.channels,
	};
	t->audio.format = f;
	t->audio.total = ai->total_samples;

	const char *filter = NULL;
	switch (ai->codec) {
	case MP4_A_ALAC:
		filter = "ac-alac.decode";
		t->audio.bitrate = ai->real_bitrate;
		break;

	case MP4_A_AAC:
		filter = "ac-aac.decode";
		if (!t->conf.stream_copy) {
			t->audio.start_delay = ai->enc_delay;
			t->audio.end_padding = ai->end_padding;
			t->audio.bitrate = ((int)ai->aac_bitrate > 0) ? ai->aac_bitrate : ai->real_bitrate;
		}
		break;

	case MP4_A_MPEG1:
		filter = "ac-mpeg.decode";
		t->audio.bitrate = (ai->aac_bitrate != 0) ? ai->aac_bitrate : 0;
		break;
	}

	if (ai->frame_samples)
		t->oaudio.mp4_frame_samples = ai->frame_samples;

	dbglog(t, "codec:%u  total:%U  delay:%u  padding:%u  br:%u"
		, ai->codec, ai->total_samples, ai->enc_delay, ai->end_padding, t->audio.bitrate);

	return filter;
}

/**
. Read .mp4 data.
. Add the appropriate audio decoding filter.
  Set decoder properties.
  The first output data block contains codec-specific configuration data.
. The subsequent blocks are audio frames.
*/
static int mp4r_decode(struct mp4_r *m, phi_track *t)
{
	enum { I_HDR, I_DATA1, I_DATA, };
	int r;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD)
		m->in = t->data_in;

	ffstr out;

	for (;;) {
	switch (m->state) {

	case I_DATA1:
	case I_DATA:
		if (t->audio.seek_req && t->audio.seek != -1) {
			t->audio.seek_req = 0;
			uint64 seek = msec_to_samples(t->audio.seek, m->rate);
			mp4read_seek(&m->mp, seek);
			dbglog(t, "seek: %Ums", t->audio.seek);
		}
		if (m->state == I_DATA1) {
			m->state = I_DATA;
			return PHI_DATA;
		}
		//fallthrough

	case I_HDR:
		r = mp4read_process(&m->mp, &m->in, &out);
		switch (r) {
		case MP4READ_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				warnlog(t, "file is incomplete");
				return PHI_DONE;
			}
			return PHI_MORE;

		case MP4READ_HEADER: {
			print_tracks(m, t);
			const struct mp4read_audio_info *ai = get_first_audio_track(m);
			if (ai == NULL) {
				errlog(t, "no audio track found");
				return PHI_ERR;
			}

			const char *filter = mp4r_info(m, t, ai);
			if (!filter) {
				errlog(t, "%s: decoding not supported", ai->codec_name);
				return PHI_ERR;
			}

			if (!t->conf.stream_copy
				&& !core->track->filter(t, core->mod(filter), 0))
				return PHI_ERR;

			m->rate = ai->format.rate;

			t->data_in = m->in;
			t->data_out = ai->codec_conf;
			m->state = I_DATA1;

			if (t->conf.info_only)
				return PHI_LASTOUT;
			continue;
		}

		case MP4READ_TAG:
			mp4_meta(m, t);
			break;

		case MP4READ_DATA:
			goto data;

		case MP4READ_DONE:
			return PHI_LASTOUT;

		case MP4READ_SEEK:
			t->input.seek = m->mp.off;
			return PHI_MORE;

		case MP4READ_WARN:
			warnlog(t, "mp4read_process(): at offset 0x%xU: %s"
				, m->mp.off, mp4read_error(&m->mp));
			break;

		case MP4READ_ERROR:
			errlog(t, "mp4read_process(): %s", mp4read_error(&m->mp));
			return PHI_ERR;
		}
		break;
	}
	}

data:
	t->audio.pos = mp4read_cursample(&m->mp);
	dbglog(t, "passing %L bytes @%U"
		, out.len, t->audio.pos);
	t->data_in = m->in;
	t->data_out = out;
	return PHI_DATA;
}

const phi_filter phi_mp4_read = {
	mp4r_create, (void*)mp4r_free, (void*)mp4r_decode,
	"mp4-read"
};
