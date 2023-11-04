/** phiola: AAC input/output.
2016, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <acodec/alib3-bridge/aac.h>
#include <ffsys/globals.h>

const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, NULL, t, __VA_ARGS__)
#define verblog(t, ...)  phi_verblog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <acodec/aac-dec.h>
#include <acodec/aac-enc.h>

static const void* aac_iface(const char *name)
{
	if (ffsz_eq(name, "decode")) return &phi_aac_dec;
	if (ffsz_eq(name, "encode")) return &phi_aac_enc;
	return NULL;
}

static const phi_mod phi_aac = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = aac_iface,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_aac;
}
