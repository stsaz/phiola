/** phiola: .ogg read
2021, Simon Zolin */

#include <avpack/ogg-read.h>

struct ogg_r {
	oggread og;
	ffstr in;
	void *trk;
	uint sample_rate;
	uint state;
	uint stmcopy :1;
};

static void ogg_log(void *udata, const char *fmt, va_list va)
{
	struct ogg_r *o = udata;
	phi_dbglogv(core, NULL, o->trk, fmt, va);
}

static void* ogg_open(phi_track *t)
{
	struct ogg_r *o = ffmem_new(struct ogg_r);
	o->trk = t;

	ffuint64 total_size = (t->input.size != ~0ULL) ? t->input.size : 0;
	oggread_open(&o->og, total_size);
	o->og.log = ogg_log;
	o->og.udata = o;

	if (t->conf.stream_copy) {
		t->data_type = "OGG";
		o->stmcopy = 1;
	}
	return o;
}

static void ogg_close(void *ctx, phi_track *t)
{
	struct ogg_r *o = ctx;
	oggread_close(&o->og);
	ffmem_free(o);
}

#define VORBIS_HEAD_STR  "\x01vorbis"
#define FLAC_HEAD_STR  "\x7f""FLAC"
#define OPUS_HEAD_STR  "OpusHead"

static int add_decoder(struct ogg_r *o, phi_track *t, ffstr data)
{
	const char *dec = NULL, *meta_filter_name = NULL;
	if (ffstr_matchz(&data, VORBIS_HEAD_STR)) {
		meta_filter_name = "format.vorbismeta";
		if (!t->conf.stream_copy && !t->conf.info_only)
			dec = "vorbis.decode";

	} else if (ffstr_matchz(&data, OPUS_HEAD_STR)) {
		meta_filter_name = "format.opusmeta";
		if (!t->conf.stream_copy && !t->conf.info_only)
			dec = "opus.decode";

	} else if (ffstr_matchz(&data, FLAC_HEAD_STR)) {
		dec = "format.flacogg";
	} else {
		errlog(t, "Unknown codec in OGG packet: %*xb"
			, ffmin(data.len, 16), data.ptr);
		return 1;
	}

	if (dec && !core->track->filter(t, core->mod(dec), 0))
		return 1;

	if (meta_filter_name && !core->track->filter(t, core->mod(meta_filter_name), 0))
		return 1;

	return 0;
}

/** Return bits/sec. */
#define pcm_brate(bytes, samples, rate) \
	FFINT_DIVSAFE((uint64)(bytes) * 8 * (rate), samples)

#define pcm_time(samples, rate)   ((uint64)(samples) * 1000 / (rate))

static uint file_bitrate(phi_track *t, oggread *og, uint sample_rate)
{
	if (t->audio.total == 0 || og->total_size == 0)
		return 0;
	return pcm_brate(og->total_size, t->audio.total, sample_rate);
}

/*
. p0.1 -> vorbis-info (sample-rate)
. p1.x -> vorbis-tags
. px.x -> vorbis-data
*/
static int ogg_decode(void *ctx, phi_track *t)
{
	enum { I_HDR, I_INFO, I_DATA, };
	struct ogg_r *o = ctx;
	int r;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (o->state == I_INFO) {
		o->state = I_DATA;
		o->sample_rate = t->audio.format.rate;
		t->audio.bitrate = file_bitrate(t, &o->og, o->sample_rate);
	}

	if (t->chain_flags & PHI_FFWD) {
		oggread_eof(&o->og, (t->data_in.len == 0));
		o->in = t->data_in;
		t->data_in.len = 0;
	}

	for (;;) {

		if (t->audio.seek_req && t->audio.seek != -1 && o->sample_rate != 0) {
			t->audio.seek_req = 0;
			oggread_seek(&o->og, msec_to_samples(t->audio.seek, o->sample_rate));
			dbglog(t, "seek: %Ums", t->audio.seek);
		}

		r = oggread_process(&o->og, &o->in, &t->data_out);
		switch (r) {
		case OGGREAD_MORE:
			if ((t->chain_flags & (PHI_FFWD | PHI_FFIRST)) == (PHI_FFWD | PHI_FFIRST)) {
				dbglog(t, "no eos page");
				return PHI_LASTOUT;
			}
			return PHI_MORE;

		case OGGREAD_HEADER:
		case OGGREAD_DATA:
			if (o->state == I_HDR) {
				o->state = I_INFO;
				t->audio.total = oggread_info(&o->og)->total_samples;
				if (0 != add_decoder(o, t, t->data_out))
					return PHI_ERR;
			}
			goto data;

		case OGGREAD_DONE:
			return PHI_LASTOUT;

		case OGGREAD_SEEK:
			t->input.seek = oggread_offset(&o->og);
			return PHI_MORE;

		case OGGREAD_ERROR:
		default:
			errlog(t, "oggread_process(): %s", oggread_error(&o->og));
			return PHI_ERR;
		}
	}

data:
	t->audio.pos = oggread_page_pos(&o->og);
	dbglog(t, "packet#%u.%u  length:%L  page-start-pos:%U"
		, (int)oggread_page_num(&o->og), (int)oggread_pkt_num(&o->og)
		, t->data_out.len
		, t->audio.pos);

	if (o->stmcopy) {
		int64 set_gpos = -1;
		const struct ogg_hdr *h = (struct ogg_hdr*)o->og.chunk.ptr;
		int page_is_last_pkt = (o->og.seg_off == h->nsegments);
		if (page_is_last_pkt) {
			t->oaudio.ogg_flush = 1;
			set_gpos = o->og.page_endpos;
			t->oaudio.ogg_granule_pos = set_gpos;
			if (o->sample_rate != 0)
				set_gpos = pcm_time(set_gpos, o->sample_rate);
		}
	}

	return PHI_DATA;
}

const phi_filter phi_ogg_read = {
	ogg_open, ogg_close, ogg_decode,
	"ogg-read"
};
