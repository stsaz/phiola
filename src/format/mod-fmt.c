/** phiola: format module */

#include <track.h>
#include <util/util.h>

const struct phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define verblog(t, ...)  phi_verblog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

static const void* fmt_mod_iface_input(const char *name);

#include <format/mmtag.h>
#include <format/detector.h>
#include <format/reader.h>
#include <format/writer.h>

extern const phi_filter
	phi_aac_adts_write,
	phi_flac_write,
	phi_ogg_write;
extern const phi_filter
	phi_ts_read,
	phi_cue_read, phi_cue_hook,
	phi_m3u_read, phi_m3u_write,
	phi_pls_read;
extern const phi_tag_if phi_tag;

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
		{ "aac",	&phi_aac_adts_write },
		{ "flac",	&phi_flac_write },
		{ "m4a",	&fmt_write },
		{ "mp3",	&fmt_write },
		{ "mp4",	&fmt_write },
		{ "ogg",	&phi_ogg_write },
		{ "opus",	&phi_ogg_write },
		{ "wav",	&fmt_write },
	};
	const void *f;
	if (NULL == (f = map_sz_vptr_findstr(mods, FF_COUNT(mods), ext))) {
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

static const void* fmt_mod_iface_input(const char *name)
{
	static const struct map_sz_vptr mods[] = {
		{ "aac",	&fmt_read },
		{ "ape",	&fmt_read },
		{ "avi",	&fmt_read },
		{ "caf",	&fmt_read },
		{ "cue",	&phi_cue_read },
		{ "flac",	&fmt_read },
		{ "m3u",	&phi_m3u_read },
		{ "m3u8",	&phi_m3u_read },
		{ "m4a",	&fmt_read },
		{ "mka",	&fmt_read },
		{ "mkv",	&fmt_read },
		{ "mp3",	&fmt_read },
		{ "mp4",	&fmt_read },
		{ "mpc",	&fmt_read },
		{ "ogg",	&fmt_read },
		{ "opus",	&fmt_read },
		{ "pls",	&phi_pls_read },
		{ "ts",		&phi_ts_read },
		{ "wav",	&fmt_read },
		{ "wv",		&fmt_read },
	};
	return map_sz_vptr_findz2(mods, FF_COUNT(mods), name);
}

static const void* fmt_mod_iface(const char *name)
{
	static const struct map_sz_vptr mods[] = {
		{ "aac",		&fmt_read },
		{ "ape",		&fmt_read },
		{ "auto-write",	&phi_auto_write },
		{ "avi",		&fmt_read },
		{ "caf",		&fmt_read },
		{ "cue",		&phi_cue_read },
		{ "cue-hook",	&phi_cue_hook },
		{ "detect",		&phi_format_detector },
		{ "flac",		&fmt_read },
		{ "flacogg",	&fmt_read },
		{ "m3u",		&phi_m3u_read },
		{ "m3u-write",	&phi_m3u_write },
		{ "mkv",		&fmt_read },
		{ "mp3",		&fmt_read },
		{ "mp4",		&fmt_read },
		{ "mpc",		&fmt_read },
		{ "ogg",		&fmt_read },
		{ "pls",		&phi_pls_read },
		{ "tag",		&phi_tag },
		{ "ts",			&phi_ts_read },
		{ "wav",		&fmt_read },
		{ "wav-write",	&fmt_write },
		{ "wv",			&fmt_read },
	};
	return map_sz_vptr_findz2(mods, FF_COUNT(mods), name);
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
