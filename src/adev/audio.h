/** phiola: Shared code for audio I/O
2020, Simon Zolin */

#pragma once
#include <util/util.h>
#include <util/aformat.h>
#include <ffaudio/audio.h>

static inline int af_eq(const struct phi_af *a, const struct phi_af *b)
{
	return (a->format == b->format
		&& a->channels == b->channels
		&& a->rate == b->rate);
}

struct fmt_pair {
	struct phi_af bad, good;
};

static inline struct phi_af* fmt_conv_find(ffvec *fmts, const struct phi_af *fmt)
{
	struct fmt_pair *it;
	FFSLICE_WALK(fmts, it) {
		if (af_eq(fmt, &it->bad))
			return &it->good;
	}
	return NULL;
}

static inline void fmt_conv_add(ffvec *fmts, const struct phi_af *bad, const struct phi_af *good)
{
	struct phi_af *g = fmt_conv_find(fmts, bad);
	if (g == NULL) {
		struct fmt_pair *p = ffvec_pushT(fmts, struct fmt_pair);
		p->bad = *bad;
		g = &p->good;
	}
	*g = *good;
}

static const ushort ffaudio_formats[] = {
	FFAUDIO_F_INT8, FFAUDIO_F_INT16, FFAUDIO_F_INT24, FFAUDIO_F_INT32, FFAUDIO_F_INT24_4,
	FFAUDIO_F_UINT8,
	FFAUDIO_F_FLOAT32, FFAUDIO_F_FLOAT64,
};
static const char ffaudio_formats_str[][8] = {
	"int8", "int16", "int24", "int32", "int24_4",
	"uint8",
	"float32", "float64",
};
static const ushort ffpcm_formats[] = {
	PHI_PCM_8, PHI_PCM_16, PHI_PCM_24, PHI_PCM_32, PHI_PCM_24_4,
	PHI_PCM_U8,
	PHI_PCM_FLOAT32, PHI_PCM_FLOAT64,
};

static inline int phi_af_to_ffaudio(uint f)
{
	int i = ffarrint16_find(ffpcm_formats, FF_COUNT(ffpcm_formats), f);
	if (i < 0)
		return -1;
	return ffaudio_formats[i];
}

static inline int ffaudio_to_phi_af(uint f)
{
	int i = ffarrint16_find(ffaudio_formats, FF_COUNT(ffaudio_formats), f);
	if (i < 0)
		return -1;
	return ffpcm_formats[i];
}

static inline const char* ffaudio_format_str(uint f)
{
	int i = ffarrint16_find(ffaudio_formats, FF_COUNT(ffaudio_formats), f);
	return ffaudio_formats_str[i];
}

/** Get device by index */
static int audio_devbyidx(const ffaudio_interface *audio, ffaudio_dev **t, uint idev, uint flags)
{
	*t = audio->dev_alloc(flags);

	for (uint i = 0;  ;  i++) {

		int r = audio->dev_next(*t);
		if (r != 0) {
			audio->dev_free(*t);
			*t = NULL;
			return r;
		}

		if (i + 1 == idev)
			break;
	}

	return 0;
}

enum ST {
	ST_TRY = 0,
	ST_OPEN = 1,
	ST_WAITING, // -> ST_SIGNALLED
	ST_SIGNALLED, // -> ST_PROCESSING || ST_FEEDING
	ST_PROCESSING, // -> ST_WAITING || ST_SIGNALLED
	ST_FEEDING, // -> ST_WAITING || ST_SIGNALLED
};
