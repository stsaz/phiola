/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

class CoreSettings {
	private Core core;

	boolean	svc_notification_disable;
	String	trash_dir;
	boolean	file_del;
	boolean	play_no_tags;
	String	codepage;
	String	pub_data_dir;
	String	plist_save_dir;
	String	quick_move_dir;

	String	rec_path; // directory for recordings
	String	rec_enc, rec_fmt;
	int		rec_channels;
	int		rec_rate;
	int		rec_bitrate;
	int		rec_buf_len_ms;
	int		rec_until_sec;
	int		rec_gain_db100;
	boolean	rec_danorm;
	boolean	rec_exclusive;
	static final String[] rec_formats = {
		"AAC-LC",
		"AAC-HE",
		"AAC-HEv2",
		"FLAC",
		"Opus",
		"Opus-VOIP"
	};

	String	conv_outext;
	int		conv_aac_quality;
	int		conv_opus_quality;
	int		conv_vorbis_quality;
	boolean	conv_copy;
	boolean	conv_file_date_preserve;
	boolean	conv_new_add_list;
	static final String[] conv_extensions = {
		"m4a",
		"opus",
		"ogg",
		"flac",
		"wav",
		"mp3",
	};
	static final String[] conv_ext_display = {
		".m4a (AAC-LC)",
		".opus (Opus)",
		".ogg (Vorbis)",
		".flac (FLAC)",
		".wav (PCM)",
		".mp3",
	};

	CoreSettings(Core core) {
		this.core = core;

		codepage = "cp1252";
		pub_data_dir = "";
		plist_save_dir = "";
		quick_move_dir = "";
		trash_dir = "Trash";

		rec_path = "";
		rec_fmt = "m4a";
		rec_enc = "AAC-LC";
		rec_bitrate = 192;
		rec_buf_len_ms = 500;
		rec_until_sec = 3600;

		conv_outext = "m4a";
		conv_aac_quality = 5;
		conv_opus_quality = 192;
		conv_vorbis_quality = 7;
	}

	String conf_write() {
		return String.format(
			"ui_svc_notfn_disable %d\n"
			+ "play_no_tags %d\n"
			+ "codepage %s\n"
			+ "op_file_delete %d\n"
			+ "op_data_dir %s\n"
			+ "op_plist_save_dir %s\n"
			+ "op_quick_move_dir %s\n"
			+ "op_trash_dir_rel %s\n"
			+ "rec_path %s\n"
			+ "rec_enc %s\n"
			+ "rec_channels %d\n"
			+ "rec_rate %d\n"
			+ "rec_bitrate %d\n"
			+ "rec_buf_len %d\n"
			+ "rec_until %d\n"
			+ "rec_danorm %d\n"
			+ "rec_gain %d\n"
			+ "rec_exclusive %d\n"
			+ "conv_outext %s\n"
			+ "conv_aac_q %d\n"
			+ "conv_opus_q %d\n"
			+ "conv_vorbis_q %d\n"
			+ "conv_copy %d\n"
			+ "conv_file_date_pres %d\n"
			+ "conv_new_add_list %d\n"
			, core.bool_to_int(svc_notification_disable)
			, core.bool_to_int(play_no_tags)
			, codepage
			, core.bool_to_int(file_del)
			, pub_data_dir
			, plist_save_dir
			, quick_move_dir
			, trash_dir
			, rec_path
			, rec_enc
			, rec_channels
			, rec_rate
			, rec_bitrate
			, rec_buf_len_ms
			, rec_until_sec
			, core.bool_to_int(rec_danorm)
			, rec_gain_db100
			, core.bool_to_int(rec_exclusive)
			, conv_outext
			, conv_aac_quality
			, conv_opus_quality
			, conv_vorbis_quality
			, core.bool_to_int(conv_copy)
			, core.bool_to_int(conv_file_date_preserve)
			, core.bool_to_int(conv_new_add_list)
			);
	}

	void set_codepage(String val) {
		if (val.equals("cp1251")
				|| val.equals("cp1252"))
			codepage = val;
		else
			codepage = "cp1252";
	}

	void normalize() {
		if (trash_dir.isEmpty())
			trash_dir = "Trash";

		if (rec_enc.equals("AAC-LC") || rec_enc.equals("AAC-HE") || rec_enc.equals("AAC-HEv2")) {
			rec_fmt = "m4a";
		} else if (rec_enc.equals("FLAC")) {
			rec_fmt = "flac";
		} else if (rec_enc.equals("Opus") || rec_enc.equals("Opus-VOIP")) {
			rec_fmt = "opus";
		} else {
			rec_fmt = "m4a";
			rec_enc = "AAC-LC";
		}

		if (rec_bitrate <= 0)
			rec_bitrate = 192;
		if (rec_buf_len_ms <= 0)
			rec_buf_len_ms = 500;
		if (rec_until_sec < 0)
			rec_until_sec = 3600;

		if (conv_outext.isEmpty())
			conv_outext = "m4a";
	}

	void conf_load(Conf.Entry[] kv) {
		svc_notification_disable = kv[Conf.UI_SVC_NOTFN_DISABLE].enabled;
		file_del = kv[Conf.OP_FILE_DELETE].enabled;
		pub_data_dir = kv[Conf.OP_DATA_DIR].value;
		plist_save_dir = kv[Conf.OP_PLIST_SAVE_DIR].value;
		quick_move_dir = kv[Conf.OP_QUICK_MOVE_DIR].value;
		trash_dir = kv[Conf.OP_TRASH_DIR_REL].value;
		play_no_tags = kv[Conf.PLAY_NO_TAGS].enabled;
		set_codepage(kv[Conf.CODEPAGE].value);

		rec_path = kv[Conf.REC_PATH].value;
		rec_enc = kv[Conf.REC_ENC].value;
		rec_channels = core.str_to_uint(kv[Conf.REC_CHANNELS].value, 0);
		rec_rate = core.str_to_uint(kv[Conf.REC_RATE].value, 0);
		rec_bitrate = core.str_to_uint(kv[Conf.REC_BITRATE].value, rec_bitrate);
		rec_buf_len_ms = core.str_to_uint(kv[Conf.REC_BUF_LEN].value, rec_buf_len_ms);
		rec_danorm = kv[Conf.REC_DANORM].enabled;
		rec_exclusive = kv[Conf.REC_EXCLUSIVE].enabled;
		rec_until_sec = core.str_to_uint(kv[Conf.REC_UNTIL].value, rec_until_sec);
		rec_gain_db100 = core.str_to_int(kv[Conf.REC_GAIN].value, rec_gain_db100);

		conv_outext = kv[Conf.CONV_OUTEXT].value;
		conv_aac_quality = core.str_to_uint(kv[Conf.CONV_AAC_Q].value, conv_aac_quality);
		conv_opus_quality = core.str_to_uint(kv[Conf.CONV_OPUS_Q].value, conv_opus_quality);
		conv_vorbis_quality = core.str_to_uint(kv[Conf.CONV_VORBIS_Q].value, conv_vorbis_quality);
		conv_copy = kv[Conf.CONV_COPY].enabled;
		conv_file_date_preserve = kv[Conf.CONV_FILE_DATE_PRES].enabled;
		conv_new_add_list = kv[Conf.CONV_NEW_ADD_LIST].enabled;
	}
}
