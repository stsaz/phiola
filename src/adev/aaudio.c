/** phiola: AAudio
2023, Simon Zolin */

#include <track.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "aaudio", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "aaudio", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "aaudio", t, __VA_ARGS__)

#include <adev/audio-rec.h>
#include <adev/audio-play.h>

struct aa_mod {
	ffaudio_buf*	abuf;
	uint			buf_len_msec;
	struct phi_af	fmt;
	phi_timer		tmr;
	audio_out*		user;
};

static struct aa_mod *mod;

static void aa_buf_close(void *param)
{
	if (!mod->abuf) return;

	dbglog(NULL, "free buffer");
	ffaaudio.free(mod->abuf);
	mod->abuf = NULL;
}

#include <adev/aaudio-play.h>
#include <adev/aaudio-rec.h>

static const void* aa_iface(const char *name)
{
	if (ffsz_eq(name, "rec")) return &phi_aaudio_rec;
	else if (ffsz_eq(name, "play")) return &phi_aaudio_play;
	return NULL;
}

static void aa_destroy()
{
	aa_buf_close(NULL);
	ffmem_free(mod);
	mod = NULL;
}

static const phi_mod phi_mod_aa = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = aa_iface,
	.close = aa_destroy,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_new(struct aa_mod);
	return &phi_mod_aa;
}
