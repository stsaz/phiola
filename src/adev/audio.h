/** phiola: Shared code for audio I/O
2020, Simon Zolin */

#include <util/util.h>
#include <ffaudio/audio.h>


#define ABUF_CLOSE_WAIT  3000


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
	FFAUDIO_F_FLOAT32, FFAUDIO_F_FLOAT64,
};
static const char ffaudio_formats_str[][8] = {
	"int8", "int16", "int24", "int32", "int24_4",
	"float32", "float64",
};
static const ushort ffpcm_formats[] = {
	PHI_PCM_8, PHI_PCM_16, PHI_PCM_24, PHI_PCM_32, PHI_PCM_24_4,
	PHI_PCM_FLOAT32, PHI_PCM_FLOAT64,
};

static inline int ffpcm_to_ffaudio(uint f)
{
	int i = ffarrint16_find(ffpcm_formats, FF_COUNT(ffpcm_formats), f);
	if (i < 0)
		return -1;
	return ffaudio_formats[i];
}

static inline int ffaudio_to_ffpcm(uint f)
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

static inline const char* pcm_format_str(uint f)
{
	int i = ffarrint16_find(ffpcm_formats, FF_COUNT(ffpcm_formats), f);
	return ffaudio_formats_str[i];
}


static inline int audio_dev_list(const phi_core *core, const ffaudio_interface *audio, struct phi_adev_ent **ents, uint flags, const char *mod_name)
{
	ffvec a = {};
	ffaudio_dev *t;
	struct phi_adev_ent *e;
	int r, rr = -1;

	uint f;
	if (flags == PHI_ADEV_PLAYBACK)
		f = FFAUDIO_DEV_PLAYBACK;
	else if (flags == PHI_ADEV_CAPTURE)
		f = FFAUDIO_DEV_CAPTURE;
	else
		return -1;
	t = audio->dev_alloc(f);

	for (;;) {
		r = audio->dev_next(t);
		if (r == 1)
			break;
		else if (r < 0) {
			phi_errlog(core, mod_name, NULL, "dev_next(): %s", audio->dev_error(t));
			goto end;
		}

		e = ffvec_zpushT(&a, struct phi_adev_ent);

		if (NULL == (e->name = ffsz_dup(audio->dev_info(t, FFAUDIO_DEV_NAME))))
			goto end;

		e->default_device = !!audio->dev_info(t, FFAUDIO_DEV_IS_DEFAULT);

		const ffuint *def_fmt = (void*)audio->dev_info(t, FFAUDIO_DEV_MIX_FORMAT);
		if (def_fmt != NULL) {
			e->default_format.format = ffaudio_to_ffpcm(def_fmt[0]);
			e->default_format.rate = def_fmt[1];
			e->default_format.channels = def_fmt[2];
		}
	}

	e = ffvec_zpushT(&a, struct phi_adev_ent);
	e->name = NULL;
	*ents = (void*)a.ptr;
	rr = a.len - 1;

end:
	audio->dev_free(t);
	if (rr < 0) {
		FFSLICE_WALK(&a, e) {
			ffmem_free(e->name);
		}
		ffvec_free(&a);
	}
	return rr;
}

static inline void audio_dev_listfree(struct phi_adev_ent *ents)
{
	struct phi_adev_ent *e;
	for (e = ents;  e->name != NULL;  e++) {
		ffmem_free(e->name);
	}
	ffmem_free(ents);
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

#include <adev/audio-rec.h>
#include <adev/audio-play.h>
