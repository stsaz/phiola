/** phiola: zstd filter
2023, Simon Zolin */

#include <track.h>
#include <zstd/zstd-ff.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)

#include <dfilter/zstd-comp.h>
#include <dfilter/zstd-decomp.h>

static const void* zstd_iface(const char *name)
{
	if (ffsz_eq(name, "compress")) return &phi_zstdw;
	else if (ffsz_eq(name, "decompress")) return &phi_zstdr;
	return NULL;
}
static const phi_mod phi_zstd = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	zstd_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_zstd;
}
