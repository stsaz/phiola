/** phiola: GUI: audio codecs
2026, Simon Zolin */

#pragma once

static const struct {
	char ext[5];
	u_char fmt;
} out_fmt[] = {
	{ "m4a",	PHI_AC_AAC },
	{ "opus",	PHI_AC_OPUS },
	{ "mp3",	PHI_AC_MP3 },
	{ "ogg",	PHI_AC_VORBIS },
	{ "flac",	0 },
	{ "wav",	0 },
};

static int out_file_ext_index(xxstr val);

/** AAC/Vorbis/Opus/MP3 encoding quality controls */
struct gui_ac {
	ffui_labelxx		laacq, lvorbisq, lopusq, lmp3q;
	ffui_editxx			eaacq, evorbisq, eopusq, emp3q;
	ffui_trackbarxx		tbaacq, tbvorbisq, tbopusq, tbmp3q;

	uint conf_aacq, conf_vorbisq, conf_opusq, conf_mp3q;

	// 16..800 by 16; 1..5
	static int aacq_value(uint progress) {
		if (progress > (800 - 16) / 16)
			return progress - (800 - 16) / 16;
		return 16 + progress * 16;
	}
	static int aacq_progress(uint q) {
		if (q <= 5)
			return (800 - 16) / 16 + q;
		return (q - 16) / 16;
	}

	// 16..496 by 16
	static int opusq_value(uint progress) { return 16 + 16 * progress; }
	static int opusq_progress(uint q) { return (q - 16) / 16; }

	// 0..10
	static int vorbisq_value(uint progress) { return progress; }
	static int vorbisq_progress(uint q) { return q; }

	// 9..0
	static int mp3q_value(uint progress) { return 9 - progress; }
	static int mp3q_progress(uint q) { return 9 - q; }

	void ui_to_conf() {
		conf_aacq = xxvec(eaacq.text()).str().uint32(0);
		conf_vorbisq = xxvec(evorbisq.text()).str().uint32(~0U);
		conf_opusq = xxvec(eopusq.text()).str().uint32(0);
		conf_mp3q = xxvec(emp3q.text()).str().uint32(~0U);
	}

	void init_from_conf() {
		xxstr_buf<100> s;
		uint n;

		n = (conf_aacq) ? conf_aacq : 5;
		eaacq.text(s.zfmt("%u", n));
		tbaacq.set(aacq_progress(n));

		n = (conf_vorbisq != ~0U) ? conf_vorbisq : 7;
		evorbisq.text(s.zfmt("%u", n));
		tbvorbisq.set(vorbisq_progress(n));

		n = (conf_opusq) ? conf_opusq : 192;
		eopusq.text(s.zfmt("%u", n));
		tbopusq.set(opusq_progress(n));

		n = (conf_mp3q != ~0U) ? conf_mp3q : 2;
		emp3q.text(s.zfmt("%u", n));
		tbmp3q.set(mp3q_progress(n));
	}

	void enable_controls(uint fmt, bool copy = 0) {
		eaacq.enable(!copy && fmt == PHI_AC_AAC);
		tbaacq.enable(!copy && fmt == PHI_AC_AAC);
		evorbisq.enable(!copy && fmt == PHI_AC_VORBIS);
		tbvorbisq.enable(!copy && fmt == PHI_AC_VORBIS);
		eopusq.enable(!copy && fmt == PHI_AC_OPUS);
		tbopusq.enable(!copy && fmt == PHI_AC_OPUS);
		emp3q.enable(!copy && fmt == PHI_AC_MP3);
		tbmp3q.enable(!copy && fmt == PHI_AC_MP3);
	}

	void on_trackbar(int id) {
		xxstr_buf<100> s;
		switch (id) {
		case A_CO_AACQ:
			eaacq.text(s.zfmt("%u", aacq_value(tbaacq.get())));  break;

		case A_CO_VORBISQ:
			evorbisq.text(s.zfmt("%u", vorbisq_value(tbvorbisq.get())));  break;

		case A_CO_OPUSQ:
			eopusq.text(s.zfmt("%u", opusq_value(tbopusq.get())));  break;

		case A_CO_MP3Q:
			emp3q.text(s.zfmt("%u", mp3q_value(tbmp3q.get())));  break;
		}
	}
};
