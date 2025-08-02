/** phiola: audio frame reader
2025, Simon Zolin */

#include <avpack/aac-read.h>
#include <avpack/ape-read.h>
#include <avpack/avi-read.h>
#include <avpack/caf-read.h>
#include <avpack/flac-read.h>
#include <avpack/mkv-read.h>
#include <avpack/mp3-read.h>
#include <avpack/mp4-read.h>
#include <avpack/mpc-read.h>
#include <avpack/ogg-codec-read.h>
#include <avpack/wav-read.h>
#include <avpack/wv-read.h>

struct fmt_rd {
	phi_track *trk;
	avpk_reader rd;
	ffstr input;
	uint sample_rate, iframe;
};

static void fmtr_log(void *opaque, const char *fmt, va_list va)
{
	struct fmt_rd *f = opaque;
	phi_dbglogv(core, NULL, f->trk, fmt, va);
}

static const struct avpkr_if *const avpk_formats[] = {
	&avpk_aac,
	&avpk_ape,
	&avpk_avi,
	&avpk_caf,
	&avpk_flac,
	&avpk_mkv,
	&avpk_mp3,
	&avpk_mp4,
	&avpk_mpc,
	&avpk_ogg,
	&avpk_wav,
	&avpk_wv,
};

static void* fmtr_open(phi_track *t)
{
	ffstr ext = {};
	if (t->conf.ifile.format) {
		ffstr_setz(&ext, file_ext_str(t->conf.ifile.format));
	} else {
		ffpath_split3_str(FFSTR_Z(t->conf.ifile.name), NULL, NULL, &ext);
	}
	if (!ext.len)
		return PHI_OPEN_ERR;

	struct fmt_rd *f = phi_track_allocT(t, struct fmt_rd);
	f->trk = t;
	struct avpk_reader_conf c = {
		.total_size = (t->input.size != ~0ULL) ? t->input.size : 0,
		.flags = (t->input.cant_seek) ? AVPKR_F_NO_SEEK : 0,
		.code_page = core->conf.code_page,
		.log = fmtr_log,
		.opaque = f,
	};

	if (ffstr_eqz(&ext, "aac") && t->conf.stream_copy) {
		ffstr fn = FFSTR_INITZ(t->conf.ofile.name), oext;
		ffstr_rsplitby(&fn, '.', NULL, &oext);
		if (ffstr_ieqz(&oext, "aac")) // .aac -> .aac copy
			c.flags |= AVPKR_F_AAC_FRAMES;
	}

	if (avpk_open(&f->rd, avpk_reader_find(ext.ptr, avpk_formats, FF_COUNT(avpk_formats)), &c)) {
		errlog(t, "avpk_open");
		return PHI_OPEN_ERR;
	}
	return f;
}

static void fmtr_close(struct fmt_rd *f, phi_track *t)
{
	avpk_close(&f->rd);
	phi_track_free(t, f);
}

static const char* fmtr_hdr(struct fmt_rd *f, phi_track *t, struct avpk_info *hdr)
{
	if (hdr->channels > 8) {
		errlog(t, "Invalid channels number");
		return NULL;
	}

	t->audio.format.rate = hdr->sample_rate;
	t->audio.format.channels = hdr->channels;
	t->audio.total = hdr->duration;
	t->audio.bitrate = (hdr->audio_bitrate) ? hdr->audio_bitrate : hdr->real_bitrate;
	t->audio.start_delay = hdr->delay;
	t->audio.end_padding = hdr->padding;

	t->audio.format.format = hdr->sample_bits;
	if (hdr->sample_float)
		t->audio.format.format |= 0x0100;
	switch (t->audio.format.format) {
	case 0:
	case PHI_PCM_8:
	case PHI_PCM_16:
	case PHI_PCM_24:
	case PHI_PCM_32:
	case PHI_PCM_U8:
	case PHI_PCM_FLOAT32:
	case PHI_PCM_FLOAT64:
		break;
	default:
		errlog(t, "Requested PCM format isn't supported");
		return NULL;
	}

	static const struct {
		char codec, mod[18], name[9];
	} decoders[] = {
		{ AVPKC_AAC,	"ac-aac.decode",	"AAC" },
		{ AVPKC_ALAC,	"ac-alac.decode",	"ALAC" },
		{ AVPKC_APE,	"ac-ape.decode",	"APE" },
		{ AVPKC_FLAC,	"ac-flac.decode",	"FLAC" },
		{ AVPKC_MP3,	"ac-mpeg.decode",	"MP3" },
		{ AVPKC_MPC,	"ac-mpc.decode",	"Musepack" },
		{ AVPKC_OPUS,	"ac-opus.decode",	"Opus" },
		{ AVPKC_PCM,	"",					"PCM" },
		{ AVPKC_VORBIS,	"ac-vorbis.decode",	"Vorbis" },
		{ AVPKC_WAVPACK,"ac-wavpack.decode","WavPack" },
	};
	uint i;
	for (i = 0;;  i++) {
		if (i == FF_COUNT(decoders)) {
			errlog(t, "Decoding is not supported: %xu", hdr->codec);
			return NULL;
		}
		if (hdr->codec == decoders[i].codec)
			break;
	}
	t->audio.decoder = decoders[i].name;

	switch (f->rd.ifa.format) {
	case AVPKF_MKV:
		if (t->conf.stream_copy)
			t->data_type = "mkv";
		if (hdr->codec == AVPKC_OPUS && t->conf.stream_copy)
			t->oaudio.ogg_gen_opus_tag = 1; // .mkv has no Opus Tags packet
		break;

	case AVPKF_MP3:
		if (t->conf.stream_copy)
			t->audio.mpeg1_vbr_scale = ((mp3read*)f->rd.ctx)->rd.info.vbr_scale + 1;
		break;

	case AVPKF_OGG:
		if (t->conf.stream_copy)
			t->data_type = "OGG";
		break;

	case AVPKF_WAV:
		if (hdr->sample_bits == 8)
			t->audio.format.format = PHI_PCM_U8;
		break;
	}

	switch (hdr->codec) {
	case AVPKC_AAC:
		if (t->conf.stream_copy)
			t->data_type = "aac";
		t->oaudio.mp4_frame_samples = 1024;
		break;

	case AVPKC_MP3:
		if (t->conf.stream_copy)
			t->data_type = "mpeg";
		break;

	case AVPKC_PCM:
		t->data_type = "pcm";
		t->audio.format.interleaved = 1;
		break;
	}

	return decoders[i].mod;
}

static int fmtr_process(struct fmt_rd *f, phi_track *t)
{
	union avpk_read_result res = {};

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}
	if (t->chain_flags & PHI_FFWD) {
		f->input = t->data_in;
	}

	ffstr *in = &f->input;
	for (;;) {

		if (t->audio.seek_req && t->audio.seek != -1 && f->sample_rate) {
			t->audio.seek_req = 0;
			uint64 samples = msec_to_samples(t->audio.seek, f->sample_rate);
			dbglog(t, "seek: %Ums", t->audio.seek);
			if (!(t->input.size == ~0ULL || t->input.cant_seek)) {
				avpk_seek(&f->rd, samples);
			} else {
				t->audio.seek = -1;
				warnlog(t, "can't seek");
			}
		}

		ffmem_zero_obj(&res);
		switch (avpk_read(&f->rd, in, &res)) {
		case AVPK_HEADER: {
			if (f->sample_rate) {
				if (!(t->audio.format.rate == res.hdr.sample_rate
					&& t->audio.format.channels == res.hdr.channels)) {
					errlog(t, "changing audio format on-the-fly is not supported");
					return PHI_ERR;
				}

				dbglog(t, "new logical stream");
				core->metaif->destroy(&t->meta);
				t->meta_changed = 1;
				t->audio.ogg_reset = 1;
				break;
			}

			const char *decoder = fmtr_hdr(f, t, &res.hdr);
			if (!decoder)
				return PHI_ERR;

			if (!t->conf.info_only
				&& !t->conf.stream_copy
				&& decoder[0]
				&& !core->track->filter(t, core->mod(decoder), 0))
				return PHI_ERR;

			f->sample_rate = res.hdr.sample_rate;
			break;
		}

		case AVPK_META:
			FF_ASSERT(res.tag.id < FF_COUNT(ffmmtag_str));
			if (res.tag.id != 0)
				ffstr_setz(&res.tag.name, ffmmtag_str[res.tag.id]);
			core->metaif->set(&t->meta, res.tag.name, res.tag.value, 0);
			break;

		case AVPK_DATA:
			if (t->conf.info_only
				&& !(res.frame.pos == ~0ULL && res.frame.duration == ~0U))
				return PHI_LASTOUT;
			goto data;

		case AVPK_SEEK:
			t->input.seek = res.seek_offset;
			return PHI_MORE;

		case AVPK_MORE:
			switch (f->rd.ifa.format) {
			case AVPKF_FLAC:
			case AVPKF_OGG:
			case AVPKF_WV:
				if (in && (t->chain_flags & PHI_FFWD) && t->data_in.len == 0) {
					in = NULL;
					continue;
				}
			}
			if (t->chain_flags & PHI_FFIRST)
				return PHI_LASTOUT;
			return PHI_MORE;

		case AVPK_FIN:
			return PHI_LASTOUT;

		case AVPK_WARNING:
			warnlog(t, "avpk_read() @0x%xU: %s"
				, res.error.offset, res.error.message);
			break;

		case AVPK_ERROR:
			errlog(t, "avpk_read() @0x%xU: %s"
				, res.error.offset, res.error.message);
			return PHI_ERR;
		}
	}

data:
	switch (f->rd.ifa.format) {
	case AVPKF_AAC:
		if (!((aacread*)f->rd.ctx)->duration && t->audio.total)
			((aacread*)f->rd.ctx)->duration = t->audio.total;
		break;

	case AVPKF_APE:
		t->audio.ape_block_samples = res.frame.duration;
		t->audio.ape_align4 = aperead_align4((aperead*)f->rd.ctx);
		break;

	case AVPKF_FLAC:
		t->audio.flac_samples = res.frame.duration;
		break;

	case AVPKF_OGG:
		t->oaudio.ogg_granule_pos = ((oggread*)f->rd.ctx)->page_endpos;
		break;
	}

	dbglog(t, "frame #%u  %d @%D  size:%L"
		, ++f->iframe, res.frame.duration, res.frame.pos, res.frame.len);
	t->audio.pos = res.frame.pos;
	t->data_out = *(ffstr*)&res.frame;
	return PHI_DATA;
}

const phi_filter fmt_read = {
	fmtr_open, (void*)fmtr_close, (void*)fmtr_process,
	"format-read"
};
