/** libsoxr wrapper */

#include "soxr-phi.h"
#include <soxr.h>
#include <stdlib.h>

struct soxr_ctx {
	soxr_t soxr;
	struct soxr_conf conf;
	unsigned i_sample_size, o_sample_size;
};

void phi_soxr_destroy(soxr_ctx *c)
{
	if (!c) return;

	soxr_delete(c->soxr);
	free(c);
}

static int sx_format(int f, int interleaved)
{
	static const char formats[][2][4] = {
		{
			{ SOXR_INT16_S, SOXR_INT32_S, 0, 0 },
			{ 0, SOXR_FLOAT32_S, 0, SOXR_FLOAT64_S },
		},
		{
			{ SOXR_INT16_I, SOXR_INT32_I, 0, 0 },
			{ 0, SOXR_FLOAT32_I, 0, SOXR_FLOAT64_I },
		},
	};
	return formats[interleaved][!!(f & 0x0100)][(f & 0xff) / 16 - 1];
}

int phi_soxr_create(soxr_ctx **pc, struct soxr_conf *conf)
{
	if (conf->channels > 8) {
		conf->error = "invalid channels";
		return -1;
	}

	soxr_ctx *c;
	if (!(c = calloc(1, sizeof(soxr_ctx))))
		return -1;

	soxr_io_spec_t io = {
		.itype = sx_format(conf->i_format, conf->i_interleaved),
		.otype = sx_format(conf->o_format, conf->o_interleaved),
		.scale = 1,
		.flags = (conf->flags & SOXR_F_NO_DITHER) ? SOXR_NO_DITHER : SOXR_TPDF,
	};

	if (!conf->quality)
		conf->quality = 20; // SOXR_20_BITQ
	soxr_quality_spec_t q = soxr_quality_spec(conf->quality / 4 - 1, SOXR_ROLLOFF_SMALL);

	soxr_error_t err;
	c->soxr = soxr_create(conf->i_rate, conf->o_rate, conf->channels, &err, &io, &q, NULL);
	if (err) {
		conf->error = soxr_strerror(err);
		goto end;
	}

	c->i_sample_size = (conf->i_format & 0xff) / 8 * conf->channels;
	c->o_sample_size = (conf->o_format & 0xff) / 8 * conf->channels;
	c->conf = *conf;
	*pc = c;
	return 0;

end:
	phi_soxr_destroy(c);
	return -1;
}

int phi_soxr_convert(soxr_ctx *c, const void *input, size_t len, size_t *off, void *output, size_t cap)
{
	void *in = (char*)input + *off;
	const void *in_v[8];
	if (input && !c->conf.i_interleaved) {
		for (unsigned i = 0;  i < c->conf.channels;  i++) {
			in_v[i] = (char*)((void**)input)[i] + *off;
		}
		in = in_v;
	}

	size_t idone, odone;
	if (soxr_process(c->soxr, in, (len - *off) / c->i_sample_size, &idone, output, cap / c->o_sample_size, &odone))
		return -1;

	*off += idone * c->i_sample_size;
	return odone * c->o_sample_size;
}
