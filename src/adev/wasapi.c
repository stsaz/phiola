/** phiola: WASAPI
2015, Simon Zolin */

#include <track.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "wasapi", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "wasapi", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "wasapi", t, __VA_ARGS__)

#include <adev/audio-dev.h>
#include <adev/audio-play.h>
#include <adev/audio-rec.h>

typedef struct wasapi_mod {
	ffaudio_buf *out;
	uint buffer_length_msec;
	phi_timer tmr;
	struct phi_af fmt;
	ffvec fmts; // struct fmt_pair[]
	audio_out *usedby;
	const phi_track *track;
	uint dev_idx;
	uint init_ok :1;
	uint excl :1;
} wasapi_mod;

static wasapi_mod *mod;

static void wasapi_buf_close()
{
	if (mod->out == NULL) return;

	dbglog(NULL, "free buffer");
	ffwasapi.free(mod->out);
	mod->out = NULL;
}

static int wasapi_init(phi_track *t)
{
	if (mod->init_ok)
		return 0;

	ffaudio_init_conf conf = {};
	if (0 != ffwasapi.init(&conf)) {
		errlog(t, "init: %s", conf.error);
		return -1;
	}

	mod->init_ok = 1;
	return 0;
}

#include <adev/wasapi-rec.h>
#include <adev/wasapi-play.h>

static void wasapi_destroy(void)
{
	if (mod == NULL)
		return;

	wasapi_buf_close();
	ffvec_free(&mod->fmts);
	ffwasapi.uninit();
	ffmem_free(mod);
	mod = NULL;
}

static int wasapi_adev_list(struct phi_adev_ent **ents, uint flags)
{
	if (0 != wasapi_init(NULL))
		return -1;

	int r;
	if (0 > (r = audio_dev_list(core, &ffwasapi, ents, flags, "wasapi")))
		return -1;
	return r;
}

const phi_adev_if phi_wasapi_adev = {
	wasapi_adev_list, audio_dev_listfree
};


static const void* wasapi_iface(const char *name)
{
	static const struct map_sz_vptr m[] = {
		{ "dev",	&phi_wasapi_adev },
		{ "play",	&phi_wasapi_play },
		{ "rec",	&phi_wasapi_rec },
		{}
	};
	return map_sz_vptr_find(m, name);
}

static const phi_mod phi_wasapi_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	wasapi_iface, wasapi_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_new(wasapi_mod);
	return &phi_wasapi_mod;
}
