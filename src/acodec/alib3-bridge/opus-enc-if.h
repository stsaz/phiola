/** Opus encoder interface
2016, Simon Zolin */

#pragma once
#include <afilter/pcm.h>
#include <ffbase/vector.h>
#include <avpack/vorbistag.h>
#include <opus/opus-phi.h>

typedef struct ffopus_enc {
	uint state;
	opus_ctx *enc;
	uint orig_sample_rate;
	uint bitrate;
	uint sample_rate;
	uint channels;
	uint complexity;
	uint bandwidth;
	uint mode;
	int err;
	ffvec buf;
	ffvec bufpcm;
	uint preskip;
	uint packet_dur; //msec

	vorbistagwrite vtag;
	uint min_tagsize;

	ffstr data;

	size_t pcmlen;
	const float *pcm;
	uint64 granulepos;

	uint fin :1;
} ffopus_enc;

#define ffopus_enc_errstr(o)  _ffopus_errstr((o)->err)

/** Add vorbis tag. */
static inline int ffopus_addtag(ffopus_enc *o, const char *name, const char *val, ffsize val_len)
{
	ffstr nm = FFSTR_INITZ(name),  d = FFSTR_INITN(val, val_len);
	return vorbistagwrite_add_name(&o->vtag, nm, d);
}

#define ffopus_enc_pos(o)  ((o)->granulepos)

int ffopus_create(ffopus_enc *o, const struct phi_af *fmt, int bitrate)
{
	int r;
	opus_encode_conf conf = {
		.channels = fmt->channels,
		.sample_rate = fmt->rate,
		.bitrate = bitrate,
		.complexity = o->complexity,
		.bandwidth = o->bandwidth,
		.application = o->mode,
	};
	if (0 != (r = opus_encode_create(&o->enc, &conf)))
		return ERR(o, r);
	o->preskip = conf.preskip;

	if (NULL == ffvec_alloc(&o->buf, OPUS_MAX_PKT, 1))
		return ERR(o, FFOPUS_ESYS);

	const char *svendor = opus_vendor();
	ffstr vendor = FFSTR_INITZ(svendor);
	vorbistagwrite_add(&o->vtag, MMTAG_VENDOR, vendor);

	if (o->packet_dur == 0)
		o->packet_dur = 40;
	o->channels = fmt->channels;
	o->sample_rate = fmt->rate;
	o->bitrate = bitrate;
	return 0;
}

void ffopus_enc_close(ffopus_enc *o)
{
	vorbistagwrite_destroy(&o->vtag);
	ffvec_free(&o->buf);
	ffvec_free(&o->bufpcm);
	opus_encode_free(o->enc);
}

/** Get approximate output file size. */
uint64 ffopus_enc_size(ffopus_enc *o, uint64 total_samples)
{
	return (total_samples / o->sample_rate + 1) * (o->bitrate / 8);
}

/** Get complete packet with Vorbis comments and padding. */
static int _ffopus_tags(ffopus_enc *o, ffstr *pkt)
{
	ffstr tags = vorbistagwrite_fin(&o->vtag);
	uint npadding = (tags.len < o->min_tagsize) ? o->min_tagsize - tags.len : 0;

	o->buf.len = 0;
	if (NULL == ffvec_realloc((ffvec*)&o->buf, FFS_LEN(FFOPUS_TAGS_STR) + tags.len + npadding, 1))
		return FFOPUS_ESYS;

	char *d = o->buf.ptr;
	ffmem_copy(d, FFOPUS_TAGS_STR, FFS_LEN(FFOPUS_TAGS_STR));
	ffsize i = FFS_LEN(FFOPUS_TAGS_STR);

	ffmem_copy(&d[i], tags.ptr, tags.len);
	i += tags.len;

	ffmem_zero(&d[i], npadding);
	o->buf.len = i + npadding;

	ffstr_setstr(pkt, &o->buf);
	return 0;
}

/**
Return enum FFVORBIS_R. */
int ffopus_encode(ffopus_enc *o)
{
	enum { W_HDR, W_TAGS, W_DATA1, W_DATA, W_DONE };
	int r;

	switch (o->state) {
	case W_HDR: {
		struct opus_hdr *h = (void*)o->buf.ptr;
		ffmem_copy(h->id, FFOPUS_HEAD_STR, 8);
		h->ver = 1;
		h->channels = o->channels;
		*(uint*)h->orig_sample_rate = ffint_le_cpu32(o->orig_sample_rate);
		*(ushort*)h->preskip = ffint_le_cpu16(o->preskip);
		ffmem_zero(h + 1, 3);
		ffstr_set(&o->data, o->buf.ptr, sizeof(struct opus_hdr) + 3);
		o->state = W_TAGS;
		return FFOPUS_RDATA;
	}

	case W_TAGS:
		if (0 != (r = _ffopus_tags(o, &o->data)))
			return ERR(o, r);
		o->state = W_DATA1;
		return FFOPUS_RDATA;

	case W_DATA1: {
		uint padding = o->preskip * pcm_size(PHI_PCM_FLOAT32, o->channels);
		if (NULL == ffvec_grow(&o->bufpcm, padding, 1))
			return ERR(o, FFOPUS_ESYS);
		ffmem_zero(o->bufpcm.ptr + o->bufpcm.len, padding);
		o->bufpcm.len = padding;
		o->state = W_DATA;
		// fallthrough
	}

	case W_DATA:
		break;

	case W_DONE:
		o->data.len = 0;
		return FFOPUS_RDONE;
	}

	uint samp_size = pcm_size(PHI_PCM_FLOAT32, o->channels);
	uint fr_samples = msec_to_samples(o->packet_dur, 48000);
	uint fr_size = fr_samples * samp_size;
	uint samples = fr_samples;
	ffstr d;
	r = ffstr_gather((ffstr*)&o->bufpcm, &o->bufpcm.cap, (void*)o->pcm, o->pcmlen, fr_size, &d);
	if (r < 0)
		return ERR(o, FFOPUS_ESYS);
	o->pcmlen -= r;
	o->pcm = (void*)((char*)o->pcm + r);
	if (d.len == 0) {
		if (!o->fin) {
			o->pcmlen = 0;
			return FFOPUS_RMORE;
		}
		uint padding = fr_size - o->bufpcm.len;
		samples = o->bufpcm.len / samp_size;
		if (NULL == ffvec_grow(&o->bufpcm, padding, 1))
			return ERR(o, FFOPUS_ESYS);
		ffmem_zero(o->bufpcm.ptr + o->bufpcm.len, padding);
		d.ptr = o->bufpcm.ptr;
		o->state = W_DONE;
	}
	o->bufpcm.len = 0;

	r = opus_encode_f(o->enc, (void*)d.ptr, fr_samples, o->buf.ptr);
	if (r < 0)
		return ERR(o, r);
	o->granulepos += samples;
	ffstr_set(&o->data, o->buf.ptr, r);
	return FFOPUS_RDATA;
}
