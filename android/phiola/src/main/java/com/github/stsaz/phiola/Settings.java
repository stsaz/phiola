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
		"FLAC",
		"Opus",
		"Opus-VOIP"
	};

	String	conv_out_dir, conv_out_name;
	String	conv_format;
	int		conv_aac_quality;
	int		conv_opus_quality;
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
		Phiola.AF_FLAC,
		0,
		0,
	};
	static final String[] conv_formats = {
		"m4a",
		"m4a/aac-he",
		"opus",
		"flac",
		"wav",
		"mp3",
	};
	static final String[] conv_extensions = {
		"m4a",
		"m4a",
		"opus",
		"flac",
		"wav",
		"mp3",
	};
	static final String[] conv_format_display = {
		".m4a (AAC-LC)",
		".m4a (AAC-HE)",
		".opus (Opus)",
		".flac (FLAC)",
		".wav (PCM)",
		".mp3 (Copy)",
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

	void conf_load(Conf.Entry[] kv) {
		svc_notification_disable = kv[Conf.UI_SVC_NOTFN_DISABLE].enabled;
		file_del = kv[Conf.OP_FILE_DELETE].enabled;
		pub_data_dir = kv[Conf.OP_DATA_DIR].value;
		library_dir = kv[Conf.OP_MLIB_DIR].value;
		plist_save_dir = kv[Conf.OP_PLIST_SAVE_DIR].value;
		trash_dir = kv[Conf.OP_TRASH_DIR_REL].value;
		deprecated_mods = kv[Conf.OP_DEPRECATED_MODS].enabled;
		set_codepage_str(kv[Conf.CODEPAGE].value);

		auto_skip_head_set(kv[Conf.PLAY_AUTO_SKIP].value);
		auto_skip_tail_set(kv[Conf.PLAY_AUTO_SKIP_TAIL].value);

		rec_path = kv[Conf.REC_PATH].value;
		rec_name_template = kv[Conf.REC_NAME].value;
		rec_enc = kv[Conf.REC_ENC].value;
		rec_channels = kv[Conf.REC_CHANNELS].number;
		rec_rate = kv[Conf.REC_RATE].number;
		rec_bitrate = kv[Conf.REC_BITRATE].number;
		rec_buf_len_ms = kv[Conf.REC_BUF_LEN].number;
		rec_danorm = kv[Conf.REC_DANORM].enabled;
		rec_exclusive = kv[Conf.REC_EXCLUSIVE].enabled;
		rec_src_unprocessed = kv[Conf.REC_SRC_UNPROC].enabled;
		rec_list_add = kv[Conf.REC_LIST_ADD].enabled;
		rec_longclick = kv[Conf.REC_LONGCLICK].enabled;
		rec_until_sec = core.str_to_uint(kv[Conf.REC_UNTIL].value, rec_until_sec);
		rec_gain_db100 = kv[Conf.REC_GAIN].number;

		conv_out_dir = kv[Conf.CONV_OUT_DIR].value;
		conv_out_name = kv[Conf.CONV_OUT_NAME].value;
		conv_format = kv[Conf.CONV_FORMAT].value;
		conv_aac_quality = kv[Conf.CONV_AAC_Q].number;
		conv_opus_quality = kv[Conf.CONV_OPUS_Q].number;
		conv_copy = kv[Conf.CONV_COPY].enabled;
		conv_file_date_preserve = kv[Conf.CONV_FILE_DATE_PRES].enabled;
		conv_new_add_list = kv[Conf.CONV_NEW_ADD_LIST].enabled;
	}
}
