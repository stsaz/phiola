/** phiola: audio filters */

#include <track.h>
#include <util/util.h>
#include <afilter/pcm.h>
#include <FFOS/ffos-extern.h>

const phi_core *core;
#define syserrlog(t, ...)  phi_syserrlog(core, NULL, t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <afilter/auto-conv.h>
#include <afilter/until.h>

extern const phi_filter
	phi_aconv,
	phi_gain,
	phi_peaks,
	phi_rtpeak;
static const void* af_iface(const char *name)
{
	static const struct map_sz_vptr mods[] = {
		{ "conv", &phi_aconv },
		{ "auto-conv", &phi_autoconv },
		{ "gain", &phi_gain },
		{ "until", &phi_until },
		{ "peaks", &phi_peaks },
		{ "rtpeak", &phi_rtpeak },
		{}
	};
	return map_sz_vptr_find(mods, name);
}

static const phi_mod phi_mod_afilter = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = af_iface,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_mod_afilter;
}
