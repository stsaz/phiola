/** phiola: audio filters */

#include <track.h>
#include <util/util.h>
#include <afilter/pcm.h>
#include <ffsys/globals.h>

const phi_core *core;
const phi_meta_if *meta_if;
#define syserrlog(t, ...)  phi_syserrlog(core, NULL, t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <afilter/auto-conv.h>
#include <afilter/silence-gen.h>
#include <afilter/skip.h>
#include <afilter/until.h>
#include <afilter/split.h>
#include <afilter/rgnorm.h>

extern const phi_filter
	phi_aconv,
	phi_rg_norm,
	phi_auto_norm,
	phi_gain,
	phi_peaks,
	phi_rtpeak;
static const void* af_iface(const char *name)
{
	static const struct map_sz_vptr mods[] = {
		{ "auto-conv",	&phi_autoconv },
		{ "auto-conv-f",&phi_autoconv_f },
		{ "auto-norm",	&phi_auto_norm },
		{ "conv",		&phi_aconv },
		{ "gain",		&phi_gain },
		{ "peaks",		&phi_peaks },
		{ "rg-norm",	&phi_rg_norm },
		{ "rtpeak",		&phi_rtpeak },
		{ "silence-gen",&phi_sil_gen },
		{ "skip",		&phi_pcm_skip },
		{ "split",		&phi_split },
		{ "until",		&phi_until },
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
