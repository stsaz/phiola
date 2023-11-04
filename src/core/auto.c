/** phiola: add filters based on configuration
2023, Simon Zolin */

#include <track.h>
#include <ffsys/file.h>
#include <ffsys/path.h>
extern phi_core *core;

#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)

static void* phi_autorec_open(phi_track *t)
{
	static const char rec_mods[][20] = {
#if defined FF_WIN
		"wasapi.rec",
		"direct-sound.rec",

#elif defined FF_BSD
		"oss.rec",

#elif defined FF_APPLE
		"coreaudio.rec",

#elif defined FF_ANDROID
		"aaudio.rec",

#else
		"pulse.rec",
		"alsa.rec",
		"jack.rec",
#endif
	};
	for (uint i = 0;  i < FF_COUNT(rec_mods);  i++) {
		const phi_filter *f;
		if (NULL != (f = core->mod(rec_mods[i]))) {

			if (!core->track->filter(t, f, 0))
				return PHI_OPEN_ERR;

			return PHI_OPEN_SKIP;
		}
	}

	errlog(t, "no available module for audio recording");
	return PHI_OPEN_ERR;
}

const phi_filter phi_autorec = {
	phi_autorec_open, NULL, NULL,
	"auto-rec"
};


static void* phi_autoplay_open(phi_track *t)
{
	static const char play_mods[][20] = {
#if defined FF_WIN
		"wasapi.play",
		"direct-sound.play",

#elif defined FF_BSD
		"oss.play",

#elif defined FF_APPLE
		"coreaudio.play",

#elif defined FF_ANDROID
		"aaudio.play",

#else
		"pulse.play",
		"alsa.play",
#endif
	};
	for (uint i = 0;  i < FF_COUNT(play_mods);  i++) {
		const phi_filter *f;
		if (NULL != (f = core->mod(play_mods[i]))) {

			if (!core->track->filter(t, f, 0))
				return PHI_OPEN_ERR;

			return PHI_OPEN_SKIP;
		}
	}

	errlog(t, "no available module for audio playback");
	return PHI_OPEN_ERR;
}

const phi_filter phi_autoplay = {
	phi_autoplay_open, NULL, NULL,
	"auto-play"
};


static void* phi_autoinput_open(phi_track *t)
{
	const char *fname = "core.file-read";

	if (ffsz_eq(t->conf.ifile.name, "@stdin")) {
		fname = "core.stdin";
	} else if (ffsz_matchz(t->conf.ifile.name, "http://")
		|| ffsz_matchz(t->conf.ifile.name, "https://")) {
		fname = "http.client";
	} else {
		fffileinfo fi;
		if (!fffile_info_path(t->conf.ifile.name, &fi)
			&& fffile_isdir(fffileinfo_attr(&fi)))
			fname = "core.dir-read";
	}

	if (!core->track->filter(t, core->mod(fname), 0))
		return PHI_OPEN_ERR;
	return PHI_OPEN_SKIP;
}

const phi_filter phi_auto_input = {
	phi_autoinput_open, NULL, NULL,
	"auto-input"
};


static void* phi_auto_out_open(phi_track *t)
{
	const char *fname = "core.file-write";
	ffstr fn;
	ffpath_splitname_str(FFSTR_Z(t->conf.ofile.name), &fn, NULL);
	if (ffstr_eqz(&fn, "@stdout"))
		fname = "core.stdout";

	if (!core->track->filter(t, core->mod(fname), 0))
		return PHI_OPEN_ERR;
	return PHI_OPEN_SKIP;
}

const phi_filter phi_auto_output = {
	phi_auto_out_open, NULL, NULL,
	"auto-output"
};
