/** phiola: audio frame writer
2025, Simon Zolin */

#include <avpack/writer.h>
#include <avpack/flac-write.h>
#include <avpack/mp3-write.h>
#include <avpack/mp4-write.h>
#include <avpack/ogg-write.h>
#include <avpack/wav-write.h>

extern const phi_filter phi_ogg_copy;

static const struct avpkw_if *const avpkw_formats[] = {
	&avpkw_flac,
	&avpkw_mp3,
	&avpkw_mp4,
	&avpkw_ogg,
	&avpkw_wav,
};

struct fmt_wr {
	avpk_writer wr;
	ffstr input;
	const struct avpkw_if *wif;
	uint state, nframe;
};

static void* fmtw_open(phi_track *t)
{
	struct fmt_wr *w = phi_track_allocT(t, struct fmt_wr);
	return w;
}

static void fmtw_close(struct fmt_wr *w, phi_track *t)
{
	avpk_writer_close(&w->wr);
	phi_track_free(t, w);
}

/** Get encoder filter name by file extension. */
static const char* ogg_enc_mod(const char *fn)
{
	ffstr name, ext;
	ffpath_splitpath(fn, ffsz_len(fn), NULL, &name);
	ffstr_rsplitby(&name, '.', NULL, &ext);
	if (ffstr_eqcz(&ext, "opus"))
		return "ac-opus.encode";
	return "ac-vorbis.encode";
}

static int fmtw_init(struct fmt_wr *w, phi_track *t)
{
	ffstr ext = {};
	ffpath_split3_output(FFSTR_Z(t->conf.ofile.name), NULL, NULL, &ext);
	if (!(w->wif = avpk_writer_find(ext.ptr, avpkw_formats, FF_COUNT(avpkw_formats)))) {
		t->error = PHI_E_OUT_FMT;
		return PHI_ERR;
	}

	switch (w->wif->format) {
	case AVPKF_FLAC:
		if (t->data_type != PHI_AC_PCM)
			goto err;
		if (!core->track->filter(t, core->mod("ac-flac.encode"), PHI_TF_PREV))
			return PHI_ERR;
		return PHI_MORE;

	case AVPKF_MP3:
		if (t->data_type == PHI_AC_PCM) {
			if (!core->track->filter(t, core->mod("ac-mp3lame.encode"), PHI_TF_PREV))
				return PHI_ERR;
			return PHI_MORE;
		} else if (t->data_type == PHI_AC_MP3) {
		} else {
			goto err;
		}
		break;

	case AVPKF_MP4:
		if (t->data_type == PHI_AC_PCM) {
			if (!core->track->filter(t, core->mod("ac-aac.encode"), PHI_TF_PREV))
				return PHI_ERR;
			return PHI_MORE;
		} else if (t->data_type == PHI_AC_AAC) {
		} else {
			goto err;
		}
		break;

	case AVPKF_OGG:
		if (t->data_type == PHI_AC_PCM) {
			const char *enc = ogg_enc_mod(t->conf.ofile.name);
			if (!core->track->filter(t, core->mod(enc), PHI_TF_PREV))
				return PHI_ERR;
			return PHI_MORE;

		} else if (t->data_type == PHI_AC_VORBIS
			|| t->data_type == PHI_AC_OPUS) {
			FF_ASSERT(t->conf.stream_copy);
			if (!core->track->filter(t, &phi_ogg_copy, 0))
				return PHI_ERR;
			t->data_out = t->data_in;
			return PHI_DONE;
		}
		goto err;

	case AVPKF_WAV:
		if (t->data_type != PHI_AC_PCM
			|| t->oaudio.format.format == PHI_PCM_8) {
			goto err;
		}
		if (!t->oaudio.format.interleaved) {
			t->oaudio.conv_format.interleaved = 1;
			return PHI_MORE;
		}
		break;
	}

	return PHI_DATA;

err:
	t->error = PHI_E_IN_FMT;
	return PHI_ERR;
}

static int fmtw_create(struct fmt_wr *w, phi_track *t)
{
	struct avpk_writer_conf ac = {
		.info = {
			.duration = (w->wif->format == AVPKF_WAV
				&& t->audio.total != ~0ULL && t->audio.total != 0
				&& t->audio.format.rate == t->oaudio.format.rate)
				? (t->audio.total - t->audio.pos) : 0,
			.sample_rate = t->oaudio.format.rate,
			.sample_bits = pcm_bits(t->oaudio.format.format),
			.sample_float = !!(t->oaudio.format.format & 0x0100),
			.channels = t->oaudio.format.channels,
			.delay = t->oaudio.mp4_delay,
			.real_bitrate = t->oaudio.mp4_bitrate,
		},
	};

	if (w->wif->format == AVPKF_FLAC) {
		if (t->audio.total != ~0ULL && t->audio.total != 0 && !t->output.cant_seek)
			ac.info.duration = (t->audio.total - t->audio.pos) * t->oaudio.format.rate / t->audio.format.rate;
		ac.info.opaque = t->data_in;

	} else if (w->wif->format == AVPKF_OGG) {
		ac.info.ogg_serial = core->rand();
		ac.info.ogg_max_page_samples = (t->oaudio.format.rate) ? t->oaudio.format.rate : 48000;
	}

	int r = avpk_create(&w->wr, w->wif, &ac);
	if (r) {
		t->error = PHI_E_OUT_FMT;
		return PHI_ERR;
	}

	switch (w->wr.ifa.format) {
	case AVPKF_MP3:
		if (t->conf.stream_copy) {
			((mp3write*)w->wr.ctx)->options |= MP3WRITE_XINGTAG;
			((mp3write*)w->wr.ctx)->vbr_scale = (int)t->audio.mpeg1_vbr_scale - 1;
		}
		break;
	}

	return PHI_DATA;
}

static void fmtw_meta(struct fmt_wr *w, phi_track *t)
{
	if (w->wif->format == AVPKF_FLAC
		&& t->oaudio.flac_vendor != NULL)
		avpk_tag(&w->wr, MMTAG_VENDOR, FFSTR_Z("vendor"), FFSTR_Z(t->oaudio.flac_vendor));
	else if (w->wif->format == AVPKF_OGG)
		return; // Tags are inside codec header packets

	uint i = 0;
	ffstr name, val;
	while (core->metaif->list(&t->meta, &i, &name, &val, PHI_META_UNIQUE)) {
		int id = ffszarr_find(ffmmtag_str, FF_COUNT(ffmmtag_str), name.ptr, name.len);
		if (avpk_tag(&w->wr, (id > 0) ? id : 0, name, val)) {
			warnlog(t, "can't add tag: %S", &name);
		}
	}
}

static int fmtw_process(struct fmt_wr *w, phi_track *t)
{
	int r;
	switch (w->state) {
	case 0:
		w->state = 1;
		if (PHI_DATA != (r = fmtw_init(w, t)))
			return r;
		// fallthrough

	case 1:
		if (PHI_DATA != (r = fmtw_create(w, t)))
			return r;
		fmtw_meta(w, t);
		w->state = 2;
		break;
	}

	if (t->chain_flags & PHI_FFWD)
		w->input = t->data_in;

	uint flags = 0;
	if (t->chain_flags & PHI_FFIRST) {
		flags |= AVPKW_F_LAST;
		if (w->wif->format == AVPKF_FLAC)
			flags |= AVPKW_F_FLAC_INFO;
	}

	if (t->audio.mp3_lametag)
		flags |= AVPKW_F_MP3_LAME;

	union avpk_write_result res;
	for (;;) {
		ffmem_zero_obj(&res);
		struct avpk_frame in = {
			.len = w->input.len,
			.ptr = w->input.ptr,
		};

		if (w->wif->format == AVPKF_FLAC) {
			in.duration = t->oaudio.flac_frame_samples;
		} else if (w->wif->format == AVPKF_OGG) {
			in.end_pos = t->oaudio.ogg_granule_pos;
			if (t->oaudio.ogg_granule_pos == 0)
				flags |= AVPKW_F_OGG_FLUSH; // Vorbis/Opus header packets are in separate OGG pages
		}

		r = avpk_write(&w->wr, &in, flags, &res);
		w->input = *(ffstr*)&in;
		switch (r) {
		case AVPK_DATA:
			goto data;

		case AVPK_SEEK:
			if (w->wr.ifa.format == AVPKF_WAV
				&& t->output.cant_seek) {
				warnlog(t, "can't seek to finalize WAV header");
				return PHI_DONE;
			}
			t->output.seek = res.seek_offset;
			continue;

		case AVPK_MORE:
			return PHI_MORE;

		case AVPK_FIN:
			if (w->wif->format == AVPKF_OGG) {
				const struct oggwrite_stat *s = &((oggwrite*)w->wr.ctx)->stat;
				verblog(t, "OGG: packets:%U, pages:%U, overhead:%.2F%%"
					, (int64)s->npkts, (int64)s->npages
					, (double)s->total_ogg * 100 / (s->total_payload + s->total_ogg));
			} else {
				verblog(t, "frames: %u", w->nframe);
			}
			return PHI_DONE;

		case AVPK_ERROR:
			errlog(t, "avpk_write(): %s", res.error.message);
			return PHI_ERR;
		}
	}

data:
	++w->nframe;
	dbglog(t, "frame #%u: %L bytes"
		, w->nframe, res.packet.len);
	t->data_out = res.packet;
	return PHI_DATA;
}

static const phi_filter fmt_write = {
	fmtw_open, (void*)fmtw_close, (void*)fmtw_process,
	"format-write"
};
