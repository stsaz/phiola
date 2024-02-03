/** WavPack decoder interface
2015, Simon Zolin */

/*
(HDR SUB_BLOCK...)...
*/

#pragma once
#include <afilter/pcm.h>
#include <ffbase/vector.h>
#include <wavpack/wavpack-phi.h>

struct ffwvpk_info {
	uint format; //enum PHI_PCM
	uint channels;
	uint sample_rate;

	uint version;
	uint comp_level;
	ffbyte md5[16];
	uint lossless :1;
};

enum FFWVPK_R {
	FFWVPK_RERR = -1,
	FFWVPK_RDATA,
	FFWVPK_RHDR,
	FFWVPK_RMORE,
};

typedef struct ffwvpack_dec {
	wavpack_ctx *wp;
	struct ffwvpk_info info;
	uint frsize;
	int hdr_done;
	const char *error;

	union {
	short *pcm;
	int *pcm32;
	float *pcmf;
	};
	uint outcap; //samples
} ffwvpack_dec;

static inline void ffwvpk_dec_open(ffwvpack_dec *w)
{
}

static inline void ffwvpk_dec_close(ffwvpack_dec *w)
{
	if (w->wp != NULL)
		wavpack_decode_free(w->wp);
	ffmem_free(w->pcm);
}

static inline const struct ffwvpk_info* ffwvpk_dec_info(ffwvpack_dec *w)
{
	return &w->info;
}

static inline const char* ffwvpk_dec_error(ffwvpack_dec *w)
{
	return w->error;
}

static int wvpk_hdrinfo(ffwvpack_dec *w, const wavpack_info *i)
{
	struct ffwvpk_info *info = &w->info;
	uint mode = i->mode;
	info->channels = i->channels;
	info->sample_rate = i->rate;

	switch (i->bps) {
	case 16:
		info->format = PHI_PCM_16;
		break;

	case 32:
		if (mode & MODE_FLOAT)
			info->format = PHI_PCM_FLOAT32;
		else
			info->format = PHI_PCM_32;
		break;

	default:
		w->error = "unsupported PCM format";
		return -1;
	}
	w->frsize = pcm_size(info->format, info->channels);

	if (mode & MODE_LOSSLESS)
		info->lossless = 1;

	if (mode & MODE_MD5)
		ffmem_copy(info->md5, i->md5, sizeof(info->md5));

	if (mode & MODE_EXTRA)
		info->comp_level = 4;
	else if (mode & MODE_VERY_HIGH)
		info->comp_level = 3;
	else if (mode & MODE_HIGH)
		info->comp_level = 2;
	else if (mode & MODE_FAST)
		info->comp_level = 1;

	return 0;
}

int ffwvpk_decode(ffwvpack_dec *w, ffstr *in, ffstr *out)
{
	int r;

	if (in->len == 0)
		return FFWVPK_RMORE;

	if (!w->hdr_done) {
		w->hdr_done = 1;
		if (NULL == (w->wp = wavpack_decode_init())) {
			w->error = "wavpack_decode_init";
			return FFWVPK_RERR;
		}
		wavpack_info info = {};
		r = wavpack_read_header(w->wp, in->ptr, in->len, &info);
		if (r == -1) {
			w->error = wavpack_errstr(w->wp);
			return FFWVPK_RERR;
		}

		if (0 != wvpk_hdrinfo(w, &info))
			return FFWVPK_RERR;

		return FFWVPK_RHDR;
	}

	if (6*4 > in->len) {
		w->error = "bad input";
		return FFWVPK_RERR;
	}

	ffuint blk_samples = ffint_le_cpu32_ptr(&in->ptr[5*4]);

	if (w->outcap < blk_samples) {
		w->pcm = ffmem_realloc(w->pcm, blk_samples * sizeof(int) * w->info.channels);
		w->outcap = blk_samples;
	}

	int n = wavpack_decode(w->wp, in->ptr, in->len, w->pcm32, w->outcap);
	if (n == -1) {
		w->error = wavpack_errstr(w->wp);
		return FFWVPK_RERR;
	} else if (n == 0) {
		return FFWVPK_RMORE;
	}

	FF_ASSERT((ffuint)n == blk_samples);

	switch (w->info.format) {
	case PHI_PCM_16:
		//in-place conversion: int[] -> short[]
		for (ffuint i = 0;  i != n * w->info.channels;  i++) {
			w->pcm[i] = (short)w->pcm32[i];
		}
		ffstr_set(out, w->pcm, n * w->frsize);
		break;

	case PHI_PCM_32:
	case PHI_PCM_FLOAT32:
		ffstr_set(out, &w->pcm32[0], n * w->frsize);
		break;

	default:
		w->error = "bad format";
		return FFWVPK_RERR;
	}

	in->len = 0;
	return FFWVPK_RDATA;
}
