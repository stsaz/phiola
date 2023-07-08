/** phiola: .mp3 read
2021, Simon Zolin */

#include <avpack/mp3-read.h>

struct mp3_r {
	mp3read mpg;
	ffstr in;
	void *trk;
	uint sample_rate;
	uint nframe;
	char codec_name[9];
	uint have_id32tag :1;
};

static void mp3_log(void *udata, const char *fmt, va_list va)
{
	struct mp3_r *m = udata;
	phi_dbglogv(core, NULL, m->trk, fmt, va);
}

static void* mp3_open(phi_track *t)
{
	struct mp3_r *m = ffmem_new(struct mp3_r);
	m->trk = t;
	ffuint64 total_size = (t->input.size != ~0ULL) ? t->input.size : 0;
	mp3read_open(&m->mpg, total_size);
	m->mpg.log = mp3_log;
	m->mpg.udata = m;
	m->mpg.id3v1.codepage = core->conf.code_page;
	m->mpg.id3v2.codepage = core->conf.code_page;
	return m;
}

static void mp3_close(struct mp3_r *m, phi_track *t)
{
	mp3read_close(&m->mpg);
	ffmem_free(m);
}

extern const phi_meta_if phi_metaif;
static void mp3_meta(struct mp3_r *m, phi_track *t, uint type)
{
	if (type == MP3READ_ID32) {
		if (!m->have_id32tag) {
			m->have_id32tag = 1;
			dbglog(t, "ID3v2.%u  size:%u"
				, id3v2read_version(&m->mpg.id3v2), id3v2read_size(&m->mpg.id3v2));
		}
	}

	ffstr name, val;
	int tag = mp3read_tag(&m->mpg, &name, &val);
	if (tag != 0)
		ffstr_setz(&name, ffmmtag_str[tag]);

	dbglog(t, "tag: %S: %S", &name, &val);
	phi_metaif.set(&t->meta, name, val);
}

static int mp3_process(struct mp3_r *m, phi_track *t)
{
	int r;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->data_in.len) {
		m->in = t->data_in;
		t->data_in.len = 0;
	}

	ffstr out;
	for (;;) {

		if (t->audio.seek_req && t->audio.seek != -1 && m->sample_rate != 0) {
			t->audio.seek_req = 0;
			mp3read_seek(&m->mpg, msec_to_samples(t->audio.seek, m->sample_rate));
			dbglog(t, "seek: %Ums", t->audio.seek);
		}

		r = mp3read_process(&m->mpg, &m->in, &out);

		switch (r) {
		case MPEG1READ_DATA:
			goto data;

		case MPEG1READ_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				return PHI_DONE;
			}
			return PHI_MORE;

		case MP3READ_DONE:
			return PHI_LASTOUT;

		case MPEG1READ_HEADER: {
			const struct mpeg1read_info *info = mp3read_info(&m->mpg);
			struct phi_af f = {
				.format = PHI_PCM_16,
				.channels = info->channels,
				.rate = info->sample_rate,
			};
			t->audio.format = f;
			m->sample_rate = info->sample_rate;
			t->audio.bitrate = info->bitrate;
			t->audio.total = info->total_samples;
			ffs_format(m->codec_name, sizeof(m->codec_name), "MPEG1-L%u%Z", info->layer);
			t->audio.decoder = m->codec_name;
			t->data_type = "mpeg";
			t->audio.mpeg1_delay = info->delay;
			t->audio.mpeg1_padding = info->padding;
			t->audio.mpeg1_vbr_scale = info->vbr_scale + 1;

			if (t->conf.info_only)
				return PHI_LASTOUT;

			if (!t->conf.stream_copy
				&& !core->track->filter(t, core->mod("mpeg.decode"), 0))
				return PHI_ERR;

			break;
		}

		case MP3READ_ID31:
		case MP3READ_ID32:
		case MP3READ_APETAG:
			mp3_meta(m, t, r);
			break;

		case MPEG1READ_SEEK:
			t->input.seek = mp3read_offset(&m->mpg);
			return PHI_MORE;

		case MP3READ_WARN:
			warnlog(t, "mp3read_read(): %s. Near sample %U, offset %U"
				, mp3read_error(&m->mpg), mp3read_cursample(&m->mpg), mp3read_offset(&m->mpg));
			break;

		case MPEG1READ_ERROR:
		default:
			errlog(t, "mp3read_read(): %s. Near sample %U, offset %U"
				, mp3read_error(&m->mpg), mp3read_cursample(&m->mpg), mp3read_offset(&m->mpg));
			return PHI_ERR;
		}
	}

data:
	t->audio.pos = mp3read_cursample(&m->mpg);
	dbglog(t, "passing frame #%u  samples:%u[%U]  size:%u  br:%u  off:%xU"
		, ++m->nframe, mpeg1_samples(out.ptr), t->audio.pos, (uint)out.len
		, mpeg1_bitrate(out.ptr), (ffint64)mp3read_offset(&m->mpg) - out.len);
	t->data_out = out;
	return PHI_DATA;
}

const phi_filter phi_mp3_read = {
	mp3_open, (void*)mp3_close, (void*)mp3_process,
	"mp3-read"
};
