/** libEBUR128 wrapper */

#include "ebur128-phi.h"
#include <ebur128.h>

struct ebur128_ctx {
	ebur128_state *es;
	unsigned channels;
};

int ebur128_open(ebur128_ctx **pc, struct ebur128_conf *conf)
{
	if (!conf->mode)
		conf->mode = EBUR128_LOUDNESS_GLOBAL;
	unsigned cm = conf->mode, mode = 0;
	if (cm & EBUR128_LOUDNESS_MOMENTARY)
		mode |= EBUR128_MODE_M;
	if (cm & EBUR128_LOUDNESS_SHORTTERM)
		mode |= EBUR128_MODE_S;
	if (cm & EBUR128_LOUDNESS_GLOBAL)
		mode |= EBUR128_MODE_I;
	if (cm & EBUR128_LOUDNESS_RANGE)
		mode |= EBUR128_MODE_LRA;
	if (cm & EBUR128_SAMPLE_PEAK)
		mode |= EBUR128_MODE_SAMPLE_PEAK;

	ebur128_ctx *c;
	if (!(c = calloc(1, sizeof(ebur128_ctx))))
		return -1;
	if (!(c->es = ebur128_init(conf->channels, conf->sample_rate, mode))) {
		free(c);
		return -1;
	}
	c->channels = conf->channels;
	*pc = c;
	return 0;
}

void ebur128_close(ebur128_ctx *c)
{
	if (!c) return;

	ebur128_destroy(&c->es);
	free(c);
}

void ebur128_process(ebur128_ctx *c, const double *data, size_t len)
{
	ebur128_add_frames_double(c->es, data, len / 8 / c->channels);
}

int ebur128_get(ebur128_ctx *c, unsigned what, void *buf, size_t cap)
{
	switch (what) {
	case EBUR128_LOUDNESS_MOMENTARY:
		if (8 > cap
			|| ebur128_loudness_momentary(c->es, buf))
			break;
		return 8;

	case EBUR128_LOUDNESS_SHORTTERM:
		if (8 > cap
			|| ebur128_loudness_shortterm(c->es, buf))
			break;
		return 8;

	case EBUR128_LOUDNESS_GLOBAL:
		if (8 > cap
			|| ebur128_loudness_global(c->es, buf))
			break;
		return 8;

	case EBUR128_LOUDNESS_RANGE:
		if (8 > cap
			|| ebur128_loudness_range(c->es, buf))
			break;
		return 8;

	case EBUR128_SAMPLE_PEAK: {
		if (8 > cap)
			break;
		double r = 0, d;
		for (unsigned i = 0;  i < c->channels;  i++) {
			if (ebur128_sample_peak(c->es, i, &d))
				return 0;
			if (d > r)
				r = d;
		}
		*(double*)buf = r;
		return 8;
	}
	}

	return 0;
}
