/** phiola: detect file format from file data
2020, Simon Zolin */

#include <track.h>

enum FILE_FORMAT {
	FILE_UNK,
	FILE_AVI,
	FILE_CAF,
	FILE_FLAC,
	FILE_M3U,
	FILE_MKV,
	FILE_MP3,
	FILE_MP4,
	FILE_OGG,
	FILE_PLS,
	FILE_WAV,
	FILE_WV,
	FILE_ID3,
};

const char file_ext[][5] = {
	"avi",
	"caf",
	"flac",
	"m3u",
	"mkv",
	"mp3",
	"mp4",
	"ogg",
	"pls",
	"wav",
	"wv",
	"",
};

/** Detect file format by first several bytes
len: >=12
Return enum FILE_FORMAT */
int file_format_detect(const void *data, ffsize len)
{
	const ffbyte *d = data;
	if (len >= 12) {
		// byte id[4]; // "RIFF"
		// byte size[4];
		// byte wave[4]; // "WAVE"
		if (!ffmem_cmp(&d[0], "RIFF", 4)
			&& !ffmem_cmp(&d[8], "WAVE", 4))
			return FILE_WAV;
	}

	if (len >= 12) {
		// byte id[4]; // "RIFF"
		// byte size[4];
		// byte wave[4]; // "AVI "
		if (!ffmem_cmp(&d[0], "RIFF", 4)
			&& !ffmem_cmp(&d[8], "AVI ", 4))
			return FILE_AVI;
	}

	if (len >= 11) {
		if (!ffmem_cmp(d, "[playlist]", 10)
			&& (d[10] == '\r' || d[10] == '\n'))
			return FILE_PLS;
	}

	if (len >= 10) {
		// id[4] // "wvpk"
		// size[4]
		// ver[2] // "XX 04"
		if (!ffmem_cmp(&d[0], "wvpk", 4)
			&& d[9] == 0x04)
			return FILE_WV;
	}

	if (len >= 8) {
		// byte size[4];
		// byte type[4]; // "ftyp"
		if (!ffmem_cmp(&d[4], "ftyp", 4)
			&& ffint_be_cpu32_ptr(&d[0]) <= 255)
			return FILE_MP4;
	}

	if (len >= 8) {
		// char caff[4]; // "caff"
		// ffbyte ver[2]; // =1
		// ffbyte flags[2]; // =0
		if (!ffmem_cmp(d, "caff\x00\x01\x00\x00", 8))
			return FILE_CAF;
	}

	if (len >= 8) {
		if (!ffmem_cmp(d, "#EXTM3U", 7)
			&& (d[7] == '\r' || d[7] == '\n'))
			return FILE_M3U;
	}

	if (len >= 5) {
		// byte sync[4]; // "OggS"
		// byte ver; // 0x0
		if (!ffmem_cmp(&d[0], "OggS", 4)
			&& d[4] == 0)
			return FILE_OGG;
	}

	if (len >= 5) {
		// byte sig[4]; // "fLaC"
		// byte last_type; // [1] [7]
		if (!ffmem_cmp(&d[0], "fLaC", 4)
			&& (d[4] & 0x7f) < 9)
			return FILE_FLAC;
	}

	if (len >= 5) {
		// ID3v2 (.mp3)
		// byte id3[3]; // "ID3"
		// byte ver[2]; // e.g. 0x3 0x0
		if (!ffmem_cmp(&d[0], "ID3", 3)
			&& d[3] <= 9
			&& d[4] <= 9)
			return FILE_ID3;
	}

	if (len >= 4) {
		// byte id[4] // 1a45dfa3
		if (!ffmem_cmp(d, "\x1a\x45\xdf\xa3", 4))
			return FILE_MKV;
	}

	if (len >= 2) {
		// byte sync1; // 0xff
		// byte sync2_ver_layer_noprotect; // [3]=0x7 [2]=0x3 [2]=0x1 [1]
		if (d[0] == 0xff && (d[1] & 0xe0) == 0xe0
			&& (d[1] & 0x18) == 0x18
			&& (d[1] & 0x06) == 2)
			return FILE_MP3;
	}

	return FILE_UNK;
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
	case FILE_UNK:
		dbglog(t, "%s: unrecognized file header", fn);
		break;

	case FILE_ID3:
		r = (ffstr_ieqz(&ext, "flac")) ? FILE_FLAC : FILE_MP3;
		// fallthrough
	default:
		ffstr_setz(&ext, file_ext[r-1]);
		dbglog(t, "detected format: %S", &ext);
	}

	const void *f = fmt_mod_iface(ext.ptr);
	if (f == NULL) {
		errlog(t, "%s: file format not supported", fn);
		t->error = PHI_E_UNKIFMT;
		return PHI_OPEN_ERR;
	}

	if (t->conf.seek_cdframes || t->conf.until_cdframes) {
		// Initial request to seek to .cue track audio position
		t->audio.seek += t->conf.seek_cdframes * 1000 / 75;
		t->audio.seek_req = (t->audio.seek > 0);

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
