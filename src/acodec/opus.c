/** phiola: Opus input/output.
2016, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <acodec/alib3-bridge/opus.h>
#include <format/mmtag.h>
#include <FFOS/ffos-extern.h>

static const phi_core *core;
static const phi_meta_if *phi_metaif;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <acodec/opus-dec.h>
#include <acodec/opus-enc.h>

static const void* opus_iface(const char *name)
{
	if (ffsz_eq(name, "decode")) return &phi_opus_dec;
	else if (ffsz_eq(name, "encode")) return &phi_opus_enc;
	return NULL;
}

static const phi_mod phi_opus_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	opus_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_opus_mod;
}
