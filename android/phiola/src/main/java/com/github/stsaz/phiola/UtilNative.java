/** phiola/Android: utils
2024, Simon Zolin */

package com.github.stsaz.phiola;

class Conf {
	Conf(Phiola phi) {}

	static class Entry {
		String value;
		int number;
		boolean enabled;
	}
	static final int
		CODEPAGE				= 1,
		CONV_AAC_Q				= CODEPAGE + 1,
		CONV_COPY				= CONV_AAC_Q + 1,
		CONV_FILE_DATE_PRES		= CONV_COPY + 1,
		CONV_FORMAT				= CONV_FILE_DATE_PRES + 1,
		CONV_NEW_ADD_LIST		= CONV_FORMAT + 1,
		CONV_OPUS_Q				= CONV_NEW_ADD_LIST + 1,
		CONV_OUT_DIR			= CONV_OPUS_Q + 1,
		CONV_OUT_NAME			= CONV_OUT_DIR + 1,
		LIST_ACTIVE				= CONV_OUT_NAME + 1,
		LIST_ADD_RM_ON_NEXT		= LIST_ACTIVE + 1,
		LIST_CURPOS				= LIST_ADD_RM_ON_NEXT + 1,
		LIST_RANDOM				= LIST_CURPOS + 1,
		LIST_REPEAT				= LIST_RANDOM + 1,
		LIST_RM_ON_ERR			= LIST_REPEAT + 1,
		LIST_RM_ON_NEXT			= LIST_RM_ON_ERR + 1,
		OP_DATA_DIR				= LIST_RM_ON_NEXT + 1,
		OP_DEPRECATED_MODS		= OP_DATA_DIR + 1,
		OP_FILE_DELETE			= OP_DEPRECATED_MODS + 1,
		OP_PLIST_SAVE_DIR		= OP_FILE_DELETE + 1,
		OP_TRASH_DIR_REL		= OP_PLIST_SAVE_DIR + 1,
		PLAY_AUTO_NORM			= OP_TRASH_DIR_REL + 1,
		PLAY_AUTO_SKIP			= PLAY_AUTO_NORM + 1,
		PLAY_AUTO_SKIP_TAIL		= PLAY_AUTO_SKIP + 1,
		PLAY_AUTO_STOP			= PLAY_AUTO_SKIP_TAIL + 1,
		PLAY_RG_NORM			= PLAY_AUTO_STOP + 1,
		REC_BITRATE				= PLAY_RG_NORM + 1,
		REC_BUF_LEN				= REC_BITRATE + 1,
		REC_CHANNELS			= REC_BUF_LEN + 1,
		REC_DANORM				= REC_CHANNELS + 1,
		REC_ENC					= REC_DANORM + 1,
		REC_EXCLUSIVE			= REC_ENC + 1,
		REC_GAIN				= REC_EXCLUSIVE + 1,
		REC_LIST_ADD			= REC_GAIN + 1,
		REC_LONGCLICK			= REC_LIST_ADD + 1,
		REC_NAME				= REC_LONGCLICK + 1,
		REC_PATH				= REC_NAME + 1,
		REC_RATE				= REC_PATH + 1,
		REC_SRC_UNPROC			= REC_RATE + 1,
		REC_UNTIL				= REC_SRC_UNPROC + 1,
		UI_CURPATH				= REC_UNTIL + 1,
		UI_FILTER_HIDE			= UI_CURPATH + 1,
		UI_INFO_IN_TITLE		= UI_FILTER_HIDE + 1,
		UI_LIST_NAMES			= UI_INFO_IN_TITLE + 1,
		UI_LIST_SCROLL_POS		= UI_LIST_NAMES + 1,
		UI_RECORD_HIDE			= UI_LIST_SCROLL_POS + 1,
		UI_SVC_NOTFN_DISABLE	= UI_RECORD_HIDE + 1,
		UI_THEME				= UI_SVC_NOTFN_DISABLE+1
		;
	native Entry[] confRead(String filepath);
	native boolean confWrite(String filepath, byte[] data);
}

class UtilNative {
	UtilNative(Phiola phi) {}

	native void storagePaths(String[] paths);
	native String trash(String trash_dir, String filepath);
	native String fileMove(String filepath, String target_dir);

	static class Files {
		String[] display_rows;
		String[] file_names;
		int n_directories;
	}
	native Files dirList(String path, int flags);
}
