/** AAC decoder interface
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
	void *pcmbuf;
	uint64 cursample;
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
static inline int ffaac_decode(ffaac *a, ffstr *output, uint64 *pos)
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

	if (rc == FFAAC_RDATA_NEWFMT) {
		a->fmt.channels = a->info.channels;
		a->fmt.rate = a->info.rate;
		if (a->contr_samprate != 0)
			a->rate_mul = a->info.rate / a->contr_samprate;
	}

	ffstr_set(output, a->pcmbuf, r * pcm_size(a->fmt.format, a->info.channels));
	*pos = a->cursample;
	return rc;
}
