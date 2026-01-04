/** libmp3lame interface
2016, Simon Zolin */

#include "lame-phi.h"
#include <lame.h>

enum {
	LAME_EMEM = 2,
	LAME_EINITPARAMS = 5,
};

static const char *const errstr[] = {
	"mp3buf was too small", //-1
	"malloc() problem", //-2
	"", //-3
	"psycho acoustic problems", //-4

	"lame_init_params() failed",
};

const char* lame_errstr(int e)
{
	e = -e - 1;
	if ((unsigned int)e >= sizeof(errstr) / sizeof(*errstr))
		return "unknown error";
	return errstr[e];
}


struct lame {
	lame_global_flags *lam;
	struct lame_params conf;
};

static void logger(const char *format, va_list ap)
{
}

int lame_create(lame **plm, struct lame_params *conf)
{
	lame *lm;

	if (NULL == (lm = calloc(1, sizeof(lame))))
		return -LAME_EMEM;

	if (NULL == (lm->lam = lame_init())) {
		free(lm);
		return -LAME_EMEM;
	}

	lame_set_errorf(lm->lam, &logger);
	lame_set_debugf(lm->lam, &logger);
	lame_set_msgf(lm->lam, &logger);

	lame_set_num_channels(lm->lam, conf->channels);
	lame_set_in_samplerate(lm->lam, conf->rate);
	lame_set_quality(lm->lam, 2);

	if (conf->quality < 10) {
		lame_set_VBR(lm->lam, vbr_default);
		lame_set_VBR_q(lm->lam, conf->quality);
	} else {
		lame_set_preset(lm->lam, conf->quality);
		lame_set_VBR(lm->lam, vbr_off);
	}

	if (-1 == lame_init_params(lm->lam)) {
		lame_free(lm);
		return -LAME_EINITPARAMS;
	}

	lm->conf = *conf;
	*plm = lm;
	return 0;
}

void lame_free(lame *lm)
{
	if (!lm) return;
	lame_close(lm->lam);
	free(lm);
}

int lame_encode(lame *lm, const void **pcm, unsigned int samples, char *buf, size_t cap)
{
	int r = 0;

	if (samples == 0) {
		return lame_encode_flush(lm->lam, (void*)buf, cap);
	}

	if (lm->conf.interleaved) {
		//16:
		r = lame_encode_buffer_interleaved(lm->lam, (void*)pcm[0], samples, (void*)buf, cap);

	} else {
		const void *ch2 = (lm->conf.channels == 1) ? NULL : pcm[1];

		switch (lm->conf.format) {
		case 16:
			r = lame_encode_buffer(lm->lam, pcm[0], ch2, samples, (void*)buf, cap);
			break;

		case 32: //float
			r = lame_encode_buffer_ieee_float(lm->lam, pcm[0], ch2, samples, (void*)buf, cap);
			break;
		}
	}

	return r;
}

int lame_lametag(lame *lm, char *buf, size_t cap)
{
	return lame_get_lametag_frame(lm->lam, (void*)buf, cap);
}

int id3tag_write_v1(lame_global_flags * gfp){}
int id3tag_write_v2(lame_global_flags * gfp){}
