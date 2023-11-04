/** phiola: WavPack input.
2015, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <acodec/alib3-bridge/wavpack.h>
#include <format/mmtag.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <acodec/wavpack-dec.h>

static const void* wvpk_iface(const char *name)
{
	if (ffsz_eq(name, "decode")) return &phi_wavpack_dec;
	return NULL;
}

static const phi_mod phi_wvpk_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	wvpk_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_wvpk_mod;
}
