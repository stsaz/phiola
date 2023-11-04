/** phiola: AAudio
2023, Simon Zolin */

#include <track.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "aaudio", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "aaudio", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "aaudio", t, __VA_ARGS__)

#include <adev/audio-rec.h>
#include <adev/aaudio-rec.h>

static const void* aa_iface(const char *name)
{
	if (ffsz_eq(name, "rec")) return &phi_aaudio_rec;
	return NULL;
}

static const phi_mod phi_mod_aa = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = aa_iface,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_mod_aa;
}
