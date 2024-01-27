/** AAC.
2016, Simon Zolin */

#pragma once
#include <afilter/pcm.h>
#include <ffbase/string.h>
#include <fdk-aac/fdk-aac-phi.h>

enum {
	AAC_ESYS = -1,
};

typedef struct ffaac {
	fdkaac_decoder *dec;
	int err;

	const char *data;
	size_t datalen;

	fdkaac_info info;
	struct phi_af fmt;
	short *pcm;
	uint pcmlen; // PCM data length in bytes
	void *pcmbuf;
	uint64 total_samples;
	uint64 cursample;
	uint64 seek_sample;
	uint enc_delay;
	uint end_padding;
	uint contr_samprate;
	uint rate_mul;
} ffaac;

enum FFAAC_R {
	FFAAC_RERR = -1,
	FFAAC_RDATA,
	FFAAC_RDATA_NEWFMT, //data with a new audio format
	FFAAC_RMORE,
	FFAAC_RDONE,
};

static inline const char* ffaac_errstr(ffaac *a)
{
	if (a->err == AAC_ESYS)
		return fferr_strptr(fferr_last());

	return fdkaac_decode_errstr(-a->err);
}

static inline int ffaac_open(ffaac *a, uint channels, const char *conf, size_t len)
{
	int r;
	if (0 != (r = fdkaac_decode_open(&a->dec, conf, len))) {
		a->err = -r;
		return FFAAC_RERR;
	}

	a->fmt.format = PHI_PCM_16;
	a->fmt.rate = a->contr_samprate;
	a->fmt.channels = channels;
	if (NULL == (a->pcmbuf = ffmem_alloc(AAC_MAXFRAMESAMPLES * pcm_size(PHI_PCM_16, AAC_MAXCHANNELS))))
		return a->err = AAC_ESYS,  FFAAC_RERR;
	a->seek_sample = a->enc_delay;
	a->rate_mul = 1;
	return 0;
}

static inline void ffaac_close(ffaac *a)
{
	if (a->dec) {
		fdkaac_decode_free(a->dec);
		a->dec = NULL;
	}
	ffmem_free(a->pcmbuf);
}

#define ffaac_input(a, d, len, pos) \
	(a)->data = (d),  (a)->datalen = (len),  (a)->cursample = (pos) * (a)->rate_mul

/**
Return enum FFAAC_R. */
static inline int ffaac_decode(ffaac *a)
{
	int r, rc;
	r = fdkaac_decode(a->dec, a->data, a->datalen, a->pcmbuf);
	if (r == 0)
		return FFAAC_RMORE;
	else if (r < 0) {
		a->err = -r;
		return FFAAC_RERR;
	}

	rc = FFAAC_RDATA;
	fdkaac_frameinfo(a->dec, &a->info);
	if (a->fmt.rate != a->info.rate
		|| a->fmt.channels != a->info.channels) {
		rc = FFAAC_RDATA_NEWFMT;
	}

	a->datalen = 0;
	a->pcm = a->pcmbuf;
	if (a->seek_sample != (uint64)-1) {
		uint skip = ffmax((int64)(a->seek_sample - a->cursample), 0);
		if (skip >= (uint)r)
			return FFAAC_RMORE;

		a->seek_sample = (uint64)-1;
		a->pcm = (void*)((char*)a->pcmbuf + skip * pcm_size(a->fmt.format, a->info.channels));
		r -= skip;
		a->cursample += skip;
	}

	if (a->cursample + r >= a->total_samples + a->enc_delay)
		r = ffmin(r - a->end_padding, r);

	if (rc == FFAAC_RDATA_NEWFMT) {
		a->fmt.channels = a->info.channels;
		a->fmt.rate = a->info.rate;
		if (a->contr_samprate != 0)
			a->rate_mul = a->info.rate / a->contr_samprate;
	}

	a->pcmlen = r * pcm_size(a->fmt.format, a->info.channels);
	return rc;
}

/** Seek on decoded frame (after a target frame is found in container). */
static inline void ffaac_seek(ffaac *a, uint64 sample)
{
	a->seek_sample = sample + a->enc_delay;
}


typedef struct ffaac_enc {
	fdkaac_encoder *enc;
	fdkaac_conf info;
	int err;

	const char *data;
	size_t datalen;
	void *buf;

	const short *pcm;
	uint pcmlen; // PCM data length in bytes

	uint fin :1;
} ffaac_enc;

static inline const char* ffaac_enc_errstr(ffaac_enc *a)
{
	if (a->err == AAC_ESYS)
		return fferr_strptr(fferr_last());

	return fdkaac_encode_errstr(-a->err);
}

static inline int ffaac_create(ffaac_enc *a, const struct phi_af *pcm, uint quality)
{
	int r;
	if (a->info.aot == 0)
		a->info.aot = AAC_LC;
	a->info.channels = pcm->channels;
	a->info.rate = pcm->rate;
	a->info.quality = quality;

	if (0 != (r = fdkaac_encode_create(&a->enc, &a->info))) {
		a->err = -r;
		return FFAAC_RERR;
	}

	if (NULL == (a->buf = ffmem_alloc(a->info.max_frame_size))) {
		a->err = AAC_ESYS;
		return FFAAC_RERR;
	}
	return 0;
}

static inline void ffaac_enc_close(ffaac_enc *a)
{
	if (a->enc) {
		fdkaac_encode_free(a->enc);
		a->enc = NULL;
	}
	ffmem_free(a->buf);
}

/**
Return enum FFAAC_R. */
static inline int ffaac_encode(ffaac_enc *a)
{
	int r;
	size_t n = a->pcmlen / (a->info.channels * sizeof(short));
	if (n == 0 && !a->fin)
		return FFAAC_RMORE;

	for (;;) {
		r = fdkaac_encode(a->enc, a->pcm, &n, a->buf);
		if (r < 0) {
			a->err = -r;
			return FFAAC_RERR;
		}

		a->pcm += n * a->info.channels,  a->pcmlen -= n * a->info.channels * sizeof(short);

		if (r == 0) {
			if (a->fin) {
				if (n != 0) {
					n = 0;
					continue;
				}
				return FFAAC_RDONE;
			}
			return FFAAC_RMORE;
		}
		break;
	}

	a->data = a->buf,  a->datalen = r;
	return FFAAC_RDATA;
}

/** Get ASC. */
static inline ffstr ffaac_enc_conf(ffaac_enc *a)
{
	ffstr s;
	ffstr_set(&s, a->info.conf, a->info.conf_len);
	return s;
}

#define ffaac_cursample(a)  ((a)->cursample - (a)->enc_delay)

/** Get audio samples per AAC frame. */
#define ffaac_enc_frame_samples(a)  ((a)->info.frame_samples)

/** Get max. size per AAC frame. */
#define ffaac_enc_frame_maxsize(a)  ((a)->info.max_frame_size)
