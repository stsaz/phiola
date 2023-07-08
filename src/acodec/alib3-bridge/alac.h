/** ALAC.
2016, Simon Zolin */

#pragma once
#include <afilter/pcm.h>
#include <ffbase/vector.h>
#include <ALAC/ALAC-ff.h>

typedef struct ffalac {
	struct alac_ctx *al;

	int err;
	char serr[32];

	struct phi_af fmt;
	uint bitrate; // 0 if unknown

	const char *data;
	size_t datalen;

	const void *pcm; // 16|20|24|32-bits interleaved
	uint pcmlen; // PCM data length in bytes
	ffvec buf;
	uint64 cursample;
	uint64 seek_sample;
	uint64 total_samples; //the last frame will be truncated to match this value
} ffalac;

enum FFALAC_R {
	FFALAC_RERR = -1,
	FFALAC_RDATA,
	FFALAC_RMORE,
};

struct alac_conf {
	ffbyte frame_length[4];
	ffbyte compatible_version;
	ffbyte bit_depth;
	ffbyte unused[3];
	ffbyte channels;
	ffbyte maxrun[2];
	ffbyte max_frame_bytes[4];
	ffbyte avg_bitrate[4];
	ffbyte sample_rate[4];
};

enum {
	ESYS = 1,
	EINIT,
};

const char* ffalac_errstr(ffalac *a)
{
	if (a->err == ESYS)
		return fferr_strptr(fferr_last());

	else if (a->err == EINIT)
		return "bad magic cookie";

	uint n = ffs_fromint(a->err, a->serr, sizeof(a->serr) - 1, FFS_INTSIGN);
	a->serr[n] = '\0';
	return a->serr;
}

/** Parse ALAC magic cookie. */
int ffalac_open(ffalac *a, const char *data, size_t len)
{
	if (NULL == (a->al = alac_init(data, len))) {
		a->err = EINIT;
		return FFALAC_RERR;
	}

	const struct alac_conf *conf = (void*)data;
	a->fmt.format = conf->bit_depth;
	a->fmt.channels = conf->channels;
	a->fmt.rate = ffint_be_cpu32_ptr(conf->sample_rate);
	a->bitrate = ffint_be_cpu32_ptr(conf->avg_bitrate);

	ffuint n = ffint_be_cpu32_ptr(conf->frame_length) * pcm_size1(&a->fmt);
	if (NULL == ffvec_alloc(&a->buf, n, 1)) {
		a->err = ESYS;
		return FFALAC_RERR;
	}
	a->total_samples = (uint64)-1;
	return 0;
}

void ffalac_close(ffalac *a)
{
	if (a->al != NULL)
		alac_free(a->al);
	ffvec_free(&a->buf);
}

/** Seek on decoded frame (after a target frame is found in container). */
void ffalac_seek(ffalac *a, uint64 sample)
{
	a->seek_sample = sample;
}

#define ffalac_cursample(a)  ((a)->cursample - (a)->pcmlen / pcm_size1(&(a)->fmt))

/**
Return enum FFALAC_R. */
int ffalac_decode(ffalac *a)
{
	int r;
	uint off = 0, samps;

	r = alac_decode(a->al, a->data, a->datalen, a->buf.ptr);
	if (r < 0) {
		a->err = r;
		return FFALAC_RERR;
	} else if (r == 0)
		return FFALAC_RMORE;

	a->datalen = 0;
	samps = r;

	if (a->seek_sample != 0) {
		off = ffmax((int64)(a->seek_sample - a->cursample), 0);
		off = ffmin(off, samps);
		a->seek_sample = 0;
	}

	a->cursample += samps;
	if (a->cursample > a->total_samples) {
		samps -= ffmin(a->cursample - a->total_samples, samps);
		a->cursample = a->total_samples;
	}

	a->pcm = a->buf.ptr + off * pcm_size1(&a->fmt);
	a->pcmlen = (samps - off) * pcm_size1(&a->fmt);
	return 0;
}
