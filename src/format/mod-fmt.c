/** phiola: format module */

#include <track.h>
#include <util/util.h>

const struct phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

static const void* fmt_mod_iface(const char *name);

#include <format/meta.h>
#include <format/detector.h>

extern const phi_filter
	phi_aac_adts_write,
	phi_flac_write,
	phi_mp3_write,
	phi_mp4_write,
	phi_ogg_write,
	phi_wav_write;
extern const phi_filter
	phi_aac_adts_read,
	phi_ape_read,
	phi_avi_read,
	phi_caf_read,
	phi_flac_read,
	phi_flacogg_read,
	phi_mkv_read,
	phi_mp3_read,
	phi_mp4_read,
	phi_mpc_read,
	phi_ogg_read,
	phi_opusmeta_read,
	phi_vorbismeta_read,
	phi_wav_read,
	phi_wv_read;
extern const phi_filter
	phi_cue_read, phi_cue_hook,
	phi_m3u_read, phi_m3u_write,
	phi_pls_read;

static void* phi_autow_open(phi_track *t) { return (void*)1; }

static int phi_autow_process(void *obj, phi_track *t)
{
	ffstr ext = {};
	ffpath_split3_output(FFSTR_Z(t->conf.ofile.name), NULL, NULL, &ext);
	if (!ext.len) {
		errlog(t, "Please specify output file extension");
		return PHI_ERR;
	}

	static const struct map_sz_vptr mods[] = {
		{ "aac", &phi_aac_adts_write },
		{ "flac", &phi_flac_write },
		{ "m4a", &phi_mp4_write },
		{ "mp3", &phi_mp3_write },
		{ "mp4", &phi_mp4_write },
		{ "ogg", &phi_ogg_write },
		{ "opus", &phi_ogg_write },
		{ "wav", &phi_wav_write },
		{}
	};
	const void *f;
	if (NULL == (f = map_sz_vptr_find(mods, ext.ptr))) {
		errlog(t, "%s: output file extension isn't supported", ext.ptr);
		return PHI_ERR;
	}

	if (!core->track->filter(t, f, 0))
		return PHI_ERR;
	t->data_out = t->data_in;
	return PHI_DONE;
}

static const struct phi_filter phi_auto_write = {
	phi_autow_open, NULL, (void*)phi_autow_process,
	"auto-write"
};

static const void* fmt_mod_iface(const char *name)
{
	static const struct map_sz_vptr mods[] = {
		{ "aac", &phi_aac_adts_read },
		{ "ape", &phi_ape_read },
		{ "auto-write", &phi_auto_write },
		{ "avi", &phi_avi_read },
		{ "caf", &phi_caf_read },
		{ "cue", &phi_cue_read },
		{ "cue-hook", &phi_cue_hook },
		{ "detect", &phi_format_detector },
		{ "flac", &phi_flac_read },
		{ "flacogg", &phi_flacogg_read },
		{ "m3u", &phi_m3u_read },
		{ "m3u-write", &phi_m3u_write },
		{ "meta", &phi_metaif },
		{ "mkv", &phi_mkv_read },
		{ "mp3", &phi_mp3_read },
		{ "mp4", &phi_mp4_read },
		{ "mpc", &phi_mpc_read },
		{ "ogg", &phi_ogg_read },
		{ "opusmeta", &phi_opusmeta_read },
		{ "pls", &phi_pls_read },
		{ "vorbismeta", &phi_vorbismeta_read },
		{ "wav", &phi_wav_read },
		{ "wv", &phi_wv_read },
		{}
	};
	return map_sz_vptr_find(mods, name);
}

static const phi_mod phi_mod_fmt = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = fmt_mod_iface,
};

FF_EXPORT const phi_mod* phi_mod_init(const struct phi_core *_core)
{
	core = _core;
	return &phi_mod_fmt;
}
