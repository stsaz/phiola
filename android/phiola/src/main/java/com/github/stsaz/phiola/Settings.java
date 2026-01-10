/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

class AutoSkip {
	int val;

	String str() {
		if (val < 0)
			return String.format("%d%%", -val);
		return Integer.toString(val);
	}

	static String user_str(int n) {
		String s = "";
		if (n < 0)
			s = String.format("%d%%", -n);
		else if (n > 0)
			s = String.format("%d sec", n);
		return s;
	}

	/** "N%", "N sec" or "N" */
	static int parse(String s) {
		if (s.isEmpty())
			return 0;
		int r;
		if (s.charAt(s.length() - 1) == '%') {
			r = Util.str_to_uint(s.substring(0, s.length() - 1), 0);
			if (r >= 100)
				r = 0;
			r = -r;
		} else {
			if (s.indexOf(" sec") > 0)
				s = s.substring(0, s.length() - 4);
			r = Util.str_to_uint(s, 0);
		}
		return r;
	}
}

class CoreSettings {
	private Core core;

	boolean	debug_logs;
	boolean	svc_notification_disable;
	String	trash_dir;
	boolean	file_del, deprecated_mods;
	String	codepage;
	int		codepage_index;
	String	pub_data_dir;
	String	plist_save_dir;
	String	library_dir;

	AutoSkip auto_skip_head, auto_skip_tail;
	void auto_skip_head_set(String s) {
		int n = AutoSkip.parse(s);
		if (n == auto_skip_head.val)
			return;
		auto_skip_head.val = n;
		core.phiola.playCmd(Phiola.PC_AUTO_SKIP_HEAD, n);
	}
	void auto_skip_tail_set(String s) {
		int n = AutoSkip.parse(s);
		if (n == auto_skip_tail.val)
			return;
		auto_skip_tail.val = n;
		core.phiola.playCmd(Phiola.PC_AUTO_SKIP_TAIL, n);
	}

	boolean equalizer_enabled;
	String equalizer;
	void equalizer_set(boolean enabled, String s) {
		if (equalizer_enabled != enabled
			|| !s.equals(equalizer))
			core.phiola.quConfStr(Phiola.QC_EQUALIZER, Util.str_choice(enabled, s, ""));
		equalizer_enabled = enabled;
		equalizer = s;
	}

	int		play_seek_fwd_percent, play_seek_back_percent;

	String	rec_path; // directory for recordings
	String	rec_name_template;
	String	rec_enc, rec_fmt;
	int		rec_channels;
	int		rec_rate;
	int		rec_bitrate;
	int		rec_buf_len_ms;
	int		rec_until_sec;
	int		rec_gain_db100;
	boolean	rec_danorm;
	boolean	rec_exclusive;
	boolean	rec_src_unprocessed;
	boolean	rec_longclick;
	boolean	rec_list_add;
	static final String[] rec_formats = {
		"AAC-LC",
		"AAC-HE",
		"AAC-HEv2",
		"MP3",
		"FLAC",
		"Opus",
		"Opus-VOIP"
	};

	String	conv_out_dir, conv_out_name;
	String	conv_format;
	int		conv_aac_quality;
	int		conv_opus_quality;
	int		conv_mp3_quality;
	boolean	conv_copy;
	boolean	conv_file_date_preserve;
	boolean	conv_new_add_list;
	static final int[] conv_sample_formats = {
		0,
		8, // PHI_PCM_8
		16, // PHI_PCM_16
		24, // PHI_PCM_24
		32, // PHI_PCM_32
	};
	static final String[] conv_sample_formats_str = {
		"As Source",
		"int8",
		"int16",
		"int24",
		"int32",
	};
	static final int[] conv_encoders = {
		Phiola.AF_AAC_LC,
		Phiola.AF_AAC_HE,
		Phiola.AF_OPUS,
		Phiola.AF_MP3,
		Phiola.AF_FLAC,
		0,
	};
	static final String[] conv_formats = {
		"m4a",
		"m4a/aac-he",
		"opus",
		"mp3",
		"flac",
		"wav",
	};
	static final String[] conv_extensions = {
		"m4a",
		"m4a",
		"opus",
		"mp3",
		"flac",
		"wav",
	};
	static final String[] conv_format_display = {
		".m4a (AAC-LC)",
		".m4a (AAC-HE)",
		".opus (Opus)",
		".mp3 (MP3)",
		".flac (FLAC)",
		".wav (PCM)",
	};

	static final String[] code_pages = {
		"win1251",
		"win1252",
	};

	CoreSettings(Core core) {
		this.core = core;

		trash_dir = "";
		codepage = "";
		pub_data_dir = "";
		plist_save_dir = "";
		library_dir = "";

		auto_skip_head = new AutoSkip();
		auto_skip_tail = new AutoSkip();
		equalizer = "";

		rec_path = "";
		rec_fmt = "";
		rec_enc = "";
		rec_bitrate = 192;
		rec_buf_len_ms = 500;
		rec_until_sec = 3600;
		rec_name_template = "";

		conv_format = "";
		conv_out_dir = "";
		conv_out_name = "";
	}

	String conf_write() {
		return String.format(
			"ui_svc_notfn_disable %d\n"
			+ "codepage %s\n"
			+ "op_file_delete %d\n"
			+ "op_data_dir %s\n"
			+ "op_mlib_dir %s\n"
			+ "op_plist_save_dir %s\n"
			+ "op_trash_dir_rel %s\n"
			+ "op_deprecated_mods %d\n"

			+ "play_auto_skip %s\n"
			+ "play_auto_skip_tail %s\n"
			+ "play_eqlz_enabled %d\n"
			+ "play_equalizer %s\n"

			+ "rec_path %s\n"
			+ "rec_name %s\n"
			+ "rec_enc %s\n"
			+ "rec_channels %d\n"
			+ "rec_rate %d\n"
			+ "rec_bitrate %d\n"
			+ "rec_buf_len %d\n"
			+ "rec_until %d\n"
			+ "rec_danorm %d\n"
			+ "rec_gain %d\n"
			+ "rec_exclusive %d\n"
			+ "rec_src_unproc %d\n"
			+ "rec_list_add %d\n"
			+ "rec_longclick %d\n"

			+ "conv_out_dir %s\n"
			+ "conv_out_name %s\n"
			+ "conv_format %s\n"
			+ "conv_aac_q %d\n"
			+ "conv_opus_q %d\n"
			+ "conv_mp3_q %d\n"
			+ "conv_copy %d\n"
			+ "conv_file_date_pres %d\n"
			+ "conv_new_add_list %d\n"
			, core.bool_to_int(svc_notification_disable)
			, codepage
			, core.bool_to_int(file_del)
			, pub_data_dir
			, library_dir
			, plist_save_dir
			, trash_dir
			, core.bool_to_int(deprecated_mods)

			, auto_skip_head.str()
			, auto_skip_tail.str()
			, core.bool_to_int(equalizer_enabled)
			, equalizer

			, rec_path
			, rec_name_template
			, rec_enc
			, rec_channels
			, rec_rate
			, rec_bitrate
			, rec_buf_len_ms
			, rec_until_sec
			, core.bool_to_int(rec_danorm)
			, rec_gain_db100
			, core.bool_to_int(rec_exclusive)
			, core.bool_to_int(rec_src_unprocessed)
			, core.bool_to_int(rec_list_add)
			, core.bool_to_int(rec_longclick)

			, conv_out_dir
			, conv_out_name
			, conv_format
			, conv_aac_quality
			, conv_opus_quality
			, conv_mp3_quality + 1
			, core.bool_to_int(conv_copy)
			, core.bool_to_int(conv_file_date_preserve)
			, core.bool_to_int(conv_new_add_list)
			);
	}

	void set_codepage_str(String val) {
		int i = Util.array_ifind(code_pages, val);
		if (i < 0)
			return;
		codepage = val;
		codepage_index = i;
	}
	void set_codepage(int i) {
		codepage_index = i;
		codepage = code_pages[i];
	}

	void normalize_convert() {
		if (conv_format.isEmpty())
			conv_format = "m4a";
		if (conv_out_dir.isEmpty())
			conv_out_dir = "@filepath";
		if (conv_out_name.isEmpty())
			conv_out_name = "@filename";
		if (conv_aac_quality <= 0)
			conv_aac_quality = 5;
		if (conv_opus_quality <= 0)
			conv_opus_quality = 192;
		if (conv_mp3_quality < 0)
			conv_mp3_quality = 2;
	}

	void normalize_rec() {
		if (rec_name_template.isEmpty())
			rec_name_template = "rec-@year@month@day-@hour@minute@second";

		switch (rec_enc) {
		case "AAC-LC":
		case "AAC-HE":
		case "AAC-HEv2":
			rec_fmt = "m4a";
			break;
		case "FLAC":
			rec_fmt = "flac";
			break;

		case "Opus":
		case "Opus-VOIP":
			rec_fmt = "opus";
			break;

		case "MP3":
			rec_fmt = "mp3";  break;

		default:
			rec_fmt = "m4a";
			rec_enc = "AAC-LC";
			break;
		}

		if (rec_bitrate <= 0)
			rec_bitrate = 192;
		if (rec_buf_len_ms <= 0)
			rec_buf_len_ms = 500;
		if (rec_until_sec < 0)
			rec_until_sec = 3600;
	}

	void normalize() {
		if (pub_data_dir.isEmpty())
			pub_data_dir = core.storage_path + "/" + Core.PUB_DATA_DIR;
		if (plist_save_dir.isEmpty())
			plist_save_dir = pub_data_dir;

		if (codepage.isEmpty())
			set_codepage_str("win1252");

		if (trash_dir.isEmpty())
			trash_dir = "Trash";

		if (play_seek_fwd_percent <= 0 || play_seek_fwd_percent > 50)
			play_seek_fwd_percent = 5;
		if (play_seek_back_percent <= 0 || play_seek_back_percent > 50)
			play_seek_back_percent = 5;

		normalize_rec();
		normalize_convert();
	}

	void conf_load(Conf c) {
		svc_notification_disable = c.enabled(Conf.UI_SVC_NOTFN_DISABLE);
		file_del = c.enabled(Conf.OP_FILE_DELETE);
		pub_data_dir = c.value(Conf.OP_DATA_DIR);
		library_dir = c.value(Conf.OP_MLIB_DIR);
		plist_save_dir = c.value(Conf.OP_PLIST_SAVE_DIR);
		trash_dir = c.value(Conf.OP_TRASH_DIR_REL);
		deprecated_mods = c.enabled(Conf.OP_DEPRECATED_MODS);
		set_codepage_str(c.value(Conf.CODEPAGE));

		auto_skip_head_set(c.value(Conf.PLAY_AUTO_SKIP));
		auto_skip_tail_set(c.value(Conf.PLAY_AUTO_SKIP_TAIL));
		equalizer_enabled = c.enabled(Conf.PLAY_EQLZ_ENABLED);
		equalizer = c.value(Conf.PLAY_EQUALIZER);

		rec_path = c.value(Conf.REC_PATH);
		rec_name_template = c.value(Conf.REC_NAME);
		rec_enc = c.value(Conf.REC_ENC);
		rec_channels = c.number(Conf.REC_CHANNELS);
		rec_rate = c.number(Conf.REC_RATE);
		rec_bitrate = c.number(Conf.REC_BITRATE);
		rec_buf_len_ms = c.number(Conf.REC_BUF_LEN);
		rec_danorm = c.enabled(Conf.REC_DANORM);
		rec_exclusive = c.enabled(Conf.REC_EXCLUSIVE);
		rec_src_unprocessed = c.enabled(Conf.REC_SRC_UNPROC);
		rec_list_add = c.enabled(Conf.REC_LIST_ADD);
		rec_longclick = c.enabled(Conf.REC_LONGCLICK);
		rec_until_sec = core.str_to_uint(c.value(Conf.REC_UNTIL), rec_until_sec);
		rec_gain_db100 = c.number(Conf.REC_GAIN);

		conv_out_dir = c.value(Conf.CONV_OUT_DIR);
		conv_out_name = c.value(Conf.CONV_OUT_NAME);
		conv_format = c.value(Conf.CONV_FORMAT);
		conv_aac_quality = c.number(Conf.CONV_AAC_Q);
		conv_opus_quality = c.number(Conf.CONV_OPUS_Q);
		conv_mp3_quality = c.number(Conf.CONV_MP3_Q) - 1;
		conv_copy = c.enabled(Conf.CONV_COPY);
		conv_file_date_preserve = c.enabled(Conf.CONV_FILE_DATE_PRES);
		conv_new_add_list = c.enabled(Conf.CONV_NEW_ADD_LIST);
	}
}
