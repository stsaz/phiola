/** phiola: CoreAudio
2018, Simon Zolin */

#include <track.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "coreaudio", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "coreaudio", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "coreaudio", t, __VA_ARGS__)

#include <adev/audio-dev.h>
#include <adev/audio-play.h>
#include <adev/audio-rec.h>

static int mod_init(phi_track *t)
{
	static int init_ok;
	if (init_ok)
		return 0;

	ffaudio_init_conf conf = {};
	conf.app_name = "phiola";
	if (0 != ffcoreaudio.init(&conf)) {
		errlog(t, "init: %s", conf.error);
		return -1;
	}

	init_ok = 1;
	return 0;
}

#include <adev/coreaudio-rec.h>
#include <adev/coreaudio-play.h>

static int coreaudio_adev_list(struct phi_adev_ent **ents, uint flags)
{
	if (0 != mod_init(NULL))
		return -1;

	int r;
	if (0 > (r = audio_dev_list(core, &ffcoreaudio, ents, flags, "coreaud")))
		return -1;
	return r;
}

static const phi_adev_if phi_coreaudio_adev = {
	.list = &coreaudio_adev_list,
	.list_free = &audio_dev_listfree,
};


static const void* coreaudio_iface(const char *name)
{
	static const struct map_sz_vptr m[] = {
		{ "dev",	&phi_coreaudio_adev },
		{ "play",	&phi_coreaudio_play },
		{ "rec",	&phi_coreaudio_rec },
		{}
	};
	return map_sz_vptr_find(m, name);
}

static void coreaudio_destroy()
{
}

static const phi_mod phi_coreaudio_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = coreaudio_iface,
	.close = coreaudio_destroy,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_coreaudio_mod;
}
