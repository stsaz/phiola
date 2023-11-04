/** phiola: CoreAudio
2018, Simon Zolin */

#include <track.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(t, "coreaudio", NULL, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(t, "coreaudio", NULL, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(t, "coreaudio", NULL, __VA_ARGS__)

#include <adev/audio-dev.h>
#include <adev/audio-play.h>
#include <adev/audio-rec.h>
#include <adev/coreaudio-rec.h>
#include <adev/coreaudio-play.h>

static void coraud_destroy(void)
{
}

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

static int coraud_adev_list(struct phi_adev_ent **ents, uint flags)
{
	if (0 != mod_init(NULL))
		return -1;

	int r;
	if (0 > (r = audio_dev_list(core, &ffcoreaudio, ents, flags, "coreaud")))
		return -1;
	return r;
}

static const phi_adev_if phi_coraud_adev = {
	.list = &coraud_adev_list,
	.listfree = &audio_dev_listfree,
};


static const void* coraud_iface(const char *name)
{
	if (!ffsz_cmp(name, "out")) {
		return &phi_coreaudio_play;
	} else if (!ffsz_cmp(name, "in")) {
		return &phi_coreaudio_rec;
	} else if (!ffsz_cmp(name, "adev")) {
		return &phi_coraud_adev;
	}
	return NULL;
}

static const phi_mod phi_coraud_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = &coraud_iface,
	.destroy = &coraud_destroy,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_coraud_mod;
}
