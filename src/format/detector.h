/** phiola: detect file format from file data
2020, Simon Zolin */

#include <track.h>
#include <avpack/detector.h>
#include <avpack/reader.h>

// enum AVPK_FORMAT
const char file_ext[][5] = {
	"aac",
	"ape",
	"avi",
	"caf",
	"flac",
	"mkv",
	"mp3",
	"mp4",
	"mpc",
	"ogg",
	"ts",
	"wav",
	"wv",

	"m3u",
	"pls",

	"",
};

const char* file_ext_str(uint i)
{
	if (i == AVPKF_ID3)
		i = AVPKF_MP3;
	if (i - 1 < FF_COUNT(file_ext))
		return file_ext[i - 1];
	return "";
}

int file_format_detect(const void *data, ffsize len)
{
	return avpk_format_detect(data, len);
}

static void* fdetcr_open(phi_track *t)
{
	ffstr ext;
	const char *fn = t->conf.ifile.name;
	ffpath_split3_str(FFSTR_Z(fn), NULL, NULL, &ext);
	if (t->data_type)
		ffstr_setz(&ext, t->data_type);

	int r = file_format_detect(t->data_in.ptr, t->data_in.len);
	switch (r) {
	case 0:
		dbglog(t, "%s: unrecognized file header", fn);
		break;

	case AVPKF_ID3:
		r = (ffstr_ieqz(&ext, "flac")) ? AVPKF_FLAC : AVPKF_MP3;
		// fallthrough
	default:
		ffstr_setz(&ext, file_ext_str(r));
		t->conf.ifile.format = r;
		dbglog(t, "detected format: %S", &ext);
	}

	const void *f = NULL;
	if (ext.len)
		f = fmt_mod_iface_input(ext.ptr);
	if (f == NULL) {
		errlog(t, "%s: file format not supported", fn);
		t->error = PHI_E_UNKIFMT;
		return PHI_OPEN_ERR;
	}

	if (t->conf.seek_cdframes || t->conf.until_cdframes) {
		// Initial request to seek to .cue track audio position
		if (t->conf.seek_cdframes) {
			if (t->audio.seek == -1)
				t->audio.seek = 0;
			t->audio.seek += (uint64)t->conf.seek_cdframes * 1000 / 75;
			t->audio.seek_req = 1;
		}

		if (!core->track->filter(t, core->mod("format.cue-hook"), 0))
			return PHI_OPEN_ERR;
	}

	if (!core->track->filter(t, f, 0))
		return PHI_OPEN_ERR;

	return PHI_OPEN_SKIP;
}

const phi_filter phi_format_detector = {
	fdetcr_open, NULL, NULL,
	"format-detector"
};
