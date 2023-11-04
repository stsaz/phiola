/** phiola: Direct Sound
2015, Simon Zolin */

#include <track.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "direct-sound", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "direct-sound", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "direct-sound", t, __VA_ARGS__)

#include <adev/audio-dev.h>
#include <adev/audio-play.h>
#include <adev/audio-rec.h>
#include <adev/directsound-rec.h>
#include <adev/directsound-play.h>

static int dsnd_adev_list(struct phi_adev_ent **ents, uint flags)
{
	int r;
	if (0 > (r = audio_dev_list(core, &ffdsound, ents, flags, "dsound")))
		return -1;
	return r;
}

static const phi_adev_if phi_directsound_dev = {
	dsnd_adev_list, audio_dev_listfree,
};


static const void* dsnd_iface(const char *name)
{
	static const struct map_sz_vptr m[] = {
		{ "dev", &phi_directsound_dev },
		{ "play", &phi_directsound_play },
		{ "rec", &phi_directsound_rec },
		{}
	};
	return map_sz_vptr_find(m, name);
}

static void dsnd_destroy(void)
{
	ffdsound.uninit();
}

static const phi_mod phi_dsnd_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	dsnd_iface, dsnd_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	ffaudio_init_conf conf = {};
	if (0 != ffdsound.init(&conf))
		return NULL;
	return &phi_dsnd_mod;
}
