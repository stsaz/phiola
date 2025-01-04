/** phiola: afilter: soxr sample rate convertor
2023, Simon Zolin */

#include <track.h>
#include <soxr/soxr-phi.h>
#include <util/aformat.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct soxr {
	soxr_ctx *soxr;
	ffstr in;
	size_t in_off;
	size_t buf_cap;
	void *buf;
	void *buf_v[8];
};

static int soxr_fmt(int pf)
{
	switch (pf) {
	case PHI_PCM_16: return SOXR_I16;
	case PHI_PCM_32: return SOXR_I32;
	case PHI_PCM_FLOAT32: return SOXR_F32;
	case PHI_PCM_FLOAT64: return SOXR_F64;
	}
	return 0;
}

static void log_pcmconv(int r, const struct phi_af *in, const struct phi_af *out, phi_track *t)
{
	int level = PHI_LOG_DEBUG;
	const char *unsupp = "";
	if (r != 0) {
		level = PHI_LOG_ERR;
		unsupp = " not supported";
	}

	char bufi[100], bufo[100];
	core->conf.log(core->conf.log_obj, level, NULL, t, "audio conversion%s: %s -> %s"
		, unsupp
		, phi_af_print(in, bufi, sizeof(bufi)), phi_af_print(out, bufo, sizeof(bufo)));
}

static void* soxr_open(phi_track *t)
{
	struct soxr *c = phi_track_allocT(t, struct soxr);
	struct phi_af oaf = t->aconv.out;
	int r;
	struct soxr_conf conf = {
		.i_rate = t->aconv.in.rate,
		.i_format = soxr_fmt(t->aconv.in.format),
		.i_interleaved = t->aconv.in.interleaved,
		.o_rate = oaf.rate,
		.o_format = soxr_fmt(oaf.format),
		.o_interleaved = oaf.interleaved,
		.channels = t->aconv.in.channels,
	};
	if ((r = phi_soxr_create(&c->soxr, &conf))
		|| (core->conf.log_level >= PHI_LOG_DEBUG)) {
		log_pcmconv(r, &t->aconv.in, &t->aconv.out, t);
		if (r) {
			dbglog(t, "phi_soxr_create(): %s", conf.error);
			goto end;
		}
	}

	// Allow no more than 16MB per 1 second of 64-bit 7.1 audio: 0x00ffffff/(64/8*8)=262143
	if (oaf.rate > 262143) {
		goto end;
	}
	uint channel_len = oaf.rate * (oaf.format & 0xff) / 8;
	c->buf_cap = channel_len * oaf.channels;
	c->buf = ffmem_alloc(c->buf_cap);
	if (!oaf.interleaved) {
		for (uint i = 0;  i < oaf.channels;  i++) {
			c->buf_v[i] = (char*)c->buf + channel_len * i;
		}
	}
	return c;

end:
	t->error = PHI_E_ACONV;
	phi_track_free(t, c);
	return PHI_OPEN_ERR;
}

static void soxr_close(struct soxr *c, phi_track *t)
{
	phi_soxr_destroy(c->soxr);
	ffmem_free(c->buf);
	phi_track_free(t, c);
}

/*
This filter converts both format and sample rate.
Previous filter must deal with channel conversion.
*/
static int soxr_conv(struct soxr *c, phi_track *t)
{
	if (t->chain_flags & PHI_FFWD) {
		c->in = t->data_in;
		c->in_off = 0;
	}
	const void *in = c->in.ptr;
	if (t->chain_flags & PHI_FFIRST) {
		if (c->in.len == c->in_off) {
			in = NULL;
			c->in.len = 0;
			c->in_off = 0;
		}
	} else if (!c->in.len) {
		return PHI_MORE;
	}
	void *out = (!c->buf_v[0]) ? c->buf : c->buf_v;
	int r;
	if (0 > (r = phi_soxr_convert(c->soxr, in, c->in.len, &c->in_off, out, c->buf_cap))) {
		errlog(t, "phi_soxr_convert()");
		return PHI_ERR;
	}

	if (!r)
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_MORE;

	ffstr_set(&t->data_out, out, r);
	return PHI_DATA;
}

const phi_filter phi_soxr = {
	soxr_open, (void*)soxr_close, (void*)soxr_conv,
	"soxr-convert"
};


static const void* soxr_mod_iface(const char *name)
{
	if (ffsz_eq(name, "conv")) return &phi_soxr;
	return NULL;
}

static const phi_mod soxr_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	soxr_mod_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &soxr_mod;
}
