/** phiola: SoXr filter
2017, Simon Zolin */

#include <track.h>
#include <FFOS/ffos-extern.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)

#include <afilter/soxr-conv.h>

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
