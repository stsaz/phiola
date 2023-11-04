/** phiola: JACK
2020, Simon Zolin */

#include <track.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "jack", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "jack", t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, "jack", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "jack", t, __VA_ARGS__)

#include <adev/audio-rec.h>

struct jack_mod {
	ffbool init_ok;
};
static struct jack_mod *mod;

static void jack_destroy(void)
{
	ffjack.uninit();
}

static int jack_initonce(phi_track *t)
{
	if (mod->init_ok)
		return 0;

	// A note for the user before using JACK library's functions
	infolog(t, "Note that the messages below may be printed by JACK library directly");

	ffaudio_init_conf conf = {};
	conf.app_name = "phiola";
	if (0 != ffjack.init(&conf)) {
		errlog(t, "init: %s", conf.error);
		return -1;
	}

	mod->init_ok = 1;
	return 0;
}

#include <adev/jack-rec.h>

static const void* jack_iface(const char *name)
{
	static const struct map_sz_vptr m[] = {
		{ "rec", &phi_jack_rec },
		{}
	};
	return map_sz_vptr_find(m, name);
}

static const phi_mod phi_mod_jack = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = jack_iface,
	.close = jack_destroy,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_new(struct jack_mod);
	return &phi_mod_jack;
}
