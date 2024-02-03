/** AAC encoder interface
2016, Simon Zolin */

#pragma once
#include <afilter/pcm.h>
#include <ffbase/string.h>
#include <fdk-aac/fdk-aac-phi.h>

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
	if (a->err == -1)
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
		a->err = -1;
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

/** Get audio samples per AAC frame. */
#define ffaac_enc_frame_samples(a)  ((a)->info.frame_samples)

/** Get max. size per AAC frame. */
#define ffaac_enc_frame_maxsize(a)  ((a)->info.max_frame_size)
