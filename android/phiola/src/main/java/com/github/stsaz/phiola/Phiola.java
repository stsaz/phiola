/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

class Phiola {
	Phiola(String libdir) {
		System.load(String.format("%s/libphiola.so", libdir));
		init(libdir);
	}
	private native void init(String libdir);
	native void destroy();
	private static boolean lib_load(String filename) {
		System.load(filename);
		return true;
	}

	static class ConfEntry {
		int id;
		String value;
	}
	static final int
		CONF_CODEPAGE				= 1,
		CONF_CONV_AAC_Q				= CONF_CODEPAGE + 1,
		CONF_CONV_COPY				= CONF_CONV_AAC_Q + 1,
		CONF_CONV_FILE_DATE_PRES	= CONF_CONV_COPY + 1,
		CONF_CONV_NEW_ADD_LIST		= CONF_CONV_FILE_DATE_PRES + 1,
		CONF_CONV_OPUS_Q			= CONF_CONV_NEW_ADD_LIST + 1,
		CONF_CONV_OUTEXT			= CONF_CONV_OPUS_Q + 1,
		CONF_CONV_VORBIS_Q			= CONF_CONV_OUTEXT + 1,
		CONF_LIST_ACTIVE			= CONF_CONV_VORBIS_Q + 1,
		CONF_LIST_ADD_RM_ON_NEXT	= CONF_LIST_ACTIVE + 1,
		CONF_LIST_CURPOS			= CONF_LIST_ADD_RM_ON_NEXT + 1,
		CONF_LIST_POS				= CONF_LIST_CURPOS + 1,
		CONF_LIST_RANDOM			= CONF_LIST_POS + 1,
		CONF_LIST_REPEAT			= CONF_LIST_RANDOM + 1,
		CONF_LIST_RM_ON_ERR			= CONF_LIST_REPEAT + 1,
		CONF_LIST_RM_ON_NEXT		= CONF_LIST_RM_ON_ERR + 1,
		CONF_OP_DATA_DIR			= CONF_LIST_RM_ON_NEXT + 1,
		CONF_OP_FILE_DELETE			= CONF_OP_DATA_DIR + 1,
		CONF_OP_PLIST_SAVE_DIR		= CONF_OP_FILE_DELETE + 1,
		CONF_OP_QUICK_MOVE_DIR		= CONF_OP_PLIST_SAVE_DIR + 1,
		CONF_OP_TRASH_DIR_REL		= CONF_OP_QUICK_MOVE_DIR + 1,
		CONF_PLAY_AUTO_SKIP			= CONF_OP_TRASH_DIR_REL + 1,
		CONF_PLAY_AUTO_SKIP_TAIL	= CONF_PLAY_AUTO_SKIP + 1,
		CONF_PLAY_AUTO_STOP			= CONF_PLAY_AUTO_SKIP_TAIL + 1,
		CONF_PLAY_NO_TAGS			= CONF_PLAY_AUTO_STOP + 1,
		CONF_REC_BITRATE			= CONF_PLAY_NO_TAGS + 1,
		CONF_REC_BUF_LEN			= CONF_REC_BITRATE + 1,
		CONF_REC_CHANNELS			= CONF_REC_BUF_LEN + 1,
		CONF_REC_DANORM				= CONF_REC_CHANNELS + 1,
		CONF_REC_ENC				= CONF_REC_DANORM + 1,
		CONF_REC_EXCLUSIVE			= CONF_REC_ENC + 1,
		CONF_REC_GAIN				= CONF_REC_EXCLUSIVE + 1,
		CONF_REC_PATH				= CONF_REC_GAIN + 1,
		CONF_REC_RATE				= CONF_REC_PATH + 1,
		CONF_REC_UNTIL				= CONF_REC_RATE + 1,
		CONF_UI_CURPATH				= CONF_REC_UNTIL + 1,
		CONF_UI_FILTER_HIDE			= CONF_UI_CURPATH + 1,
		CONF_UI_INFO_IN_TITLE		= CONF_UI_FILTER_HIDE + 1,
		CONF_UI_RECORD_HIDE			= CONF_UI_INFO_IN_TITLE + 1,
		CONF_UI_STATE_HIDE			= CONF_UI_RECORD_HIDE + 1,
		CONF_UI_SVC_NOTFN_DISABLE	= CONF_UI_STATE_HIDE + 1,
		CONF_UI_THEME				= CONF_UI_SVC_NOTFN_DISABLE+1
		;
	native ConfEntry[] confRead(String filepath);
	native boolean confWrite(String filepath, byte[] data);

	native String version();
	native void setCodepage(String codepage);

	static class Meta {
		int length_msec;
		String url, artist, title, date, info;
		String[] meta;
	}
	interface MetaCallback {
		void on_finish(Meta meta);
	}
	native int meta(long q, int list_item, String filepath, MetaCallback cb);

	static class ConvertParams {
		ConvertParams() {
			from_msec = "";
			to_msec = "";
		}

		static final int F_DATE_PRESERVE = 1;
		static final int F_OVERWRITE = 2;
		int flags;

		String from_msec, to_msec;
		boolean copy;
		int sample_rate;
		int aac_quality;
		int opus_quality;
		int vorbis_quality;
	}
	interface ConvertCallback {
		void on_finish(String result);
	}
	native int convert(String iname, String oname, ConvertParams conf, ConvertCallback cb);

	static class RecordParams {
		static final int
			REC_AACLC = 0,
			REC_AACHE = 1,
			REC_AACHE2 = 2,
			REC_FLAC = 3,
			REC_OPUS = 4,
			REC_OPUS_VOICE = 5;
		int format;

		int channels;
		int sample_rate;

		static final int
			RECF_EXCLUSIVE = 1,
			RECF_POWER_SAVE = 2,
			RECF_DANORM = 4;
		int flags;

		int buf_len_msec;
		int gain_db100;
		int quality;
		int until_sec;
	}
	interface RecordCallback {
		void on_finish();
	}
	native long recStart(String oname, RecordParams conf, RecordCallback cb);
	native void recStop(long trk);

	// track queue
	native long quNew();
	native void quDestroy(long q);

	static final int QUADD_RECURSE = 1;
	native void quAdd(long q, String[] urls, int flags);

	native String quEntry(long q, int i);

	static final int QUCOM_CLEAR = 1;
	static final int QUCOM_REMOVE_I = 2;
	static final int QUCOM_COUNT = 3;

	/** Convert track index in the currently visible (filtered) list
	 to the index within its parent (not filtered) list */
	static final int QUCOM_INDEX = 4;

	static final int QUCOM_SORT = 5;
	static final int QU_SORT_FILENAME = 0;
	static final int QU_SORT_RANDOM = 1;

	native int quCmd(long q, int cmd, int i);

	native Meta quMeta(long q, int i);

	static final int QUFILTER_URL = 1;
	static final int QUFILTER_META = 2;
	native long quFilter(long q, String filter, int flags);

	/** Load playlist from a file on disk */
	native int quLoad(long q, String filepath);

	native boolean quSave(long q, String filepath);

	String[] storage_paths;
	native String trash(String trash_dir, String filepath);
	native String fileMove(String filepath, String target_dir);
}
