/** phiola: MPEG Layer3 encode
2015, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <ffsys/globals.h>
const phi_core *core;

#define syserrlog(t, ...)  phi_syserrlog(core, NULL, t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <acodec/mp3lame-enc.h>

static const void* mp3lame_iface(const char *name)
{
	if (ffsz_eq(name, "encode")) return &phi_mpeg_enc;
	return NULL;
}

static const phi_mod phi_mp3lame_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	mp3lame_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_mp3lame_mod;
}
