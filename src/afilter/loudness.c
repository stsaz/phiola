/** phiola: loudness analyzer
2024, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <afilter/pcm.h>
#include <EBUR128/ebur128-phi.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define userlog(t, ...)  phi_userlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct loudness {
	ebur128_ctx *ebur128;
};

static void* loudness_open(phi_track *t)
{
	if (!(t->oaudio.format.interleaved
		&& t->oaudio.format.format == PHI_PCM_FLOAT64)) {
		errlog(t, "input audio format not supported");
		return PHI_OPEN_ERR;
	}
	struct loudness *c = phi_track_allocT(t, struct loudness);
	struct ebur128_conf conf = {
		.channels = t->oaudio.format.channels,
		.sample_rate = t->oaudio.format.rate,
		.mode = EBUR128_LOUDNESS_MOMENTARY | EBUR128_LOUDNESS_GLOBAL,
	};
	if (ebur128_open(&c->ebur128, &conf)) {
		errlog(t, "ebur128_open()");
		phi_track_free(t, c);
		return PHI_OPEN_ERR;
	}
	return c;
}

static void loudness_close(void *f, phi_track *t)
{
	struct loudness *c = f;
	ebur128_close(c->ebur128);
	phi_track_free(t, c);
}

static int loudness_process(void *f, phi_track *t)
{
	struct loudness *c = f;
	t->data_out = t->data_in;

	ebur128_process(c->ebur128, (double*)t->data_in.ptr, t->data_in.len);

	double global, momentary;
	ebur128_get(c->ebur128, EBUR128_LOUDNESS_GLOBAL, &global, 8);
	ebur128_get(c->ebur128, EBUR128_LOUDNESS_MOMENTARY, &momentary, 8);
	t->oaudio.loudness = global;
	t->oaudio.loudness_momentary = momentary;
	dbglog(t, "loudness: %f %f", global, momentary);

	if (t->chain_flags & PHI_FFIRST) {
		if (t->conf.afilter.loudness_summary)
			userlog(t, "Loudness: %f", global);
		return PHI_DONE;
	}
	return PHI_OK;
}

static const phi_filter phi_loudness = {
	loudness_open, loudness_close, loudness_process,
	"loudness"
};


static const void* loudness_iface(const char *name)
{
	if (ffsz_eq(name, "analyze")) return &phi_loudness;
	return NULL;
}

static const phi_mod phi_mod_loudness = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	loudness_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_mod_loudness;
}
