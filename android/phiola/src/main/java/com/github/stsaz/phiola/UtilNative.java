/** phiola/Android: utils
2024, Simon Zolin */

package com.github.stsaz.phiola;

class Conf {
	Conf(Phiola phi) {}

	static class Entry {
		int id;
		String value;
	}
	static final int
		CODEPAGE				= 1,
		CONV_AAC_Q				= CODEPAGE + 1,
		CONV_COPY				= CONV_AAC_Q + 1,
		CONV_FILE_DATE_PRES		= CONV_COPY + 1,
		CONV_NEW_ADD_LIST		= CONV_FILE_DATE_PRES + 1,
		CONV_OPUS_Q				= CONV_NEW_ADD_LIST + 1,
		CONV_OUTEXT				= CONV_OPUS_Q + 1,
		CONV_VORBIS_Q			= CONV_OUTEXT + 1,
		LIST_ACTIVE				= CONV_VORBIS_Q + 1,
		LIST_ADD_RM_ON_NEXT		= LIST_ACTIVE + 1,
		LIST_CURPOS				= LIST_ADD_RM_ON_NEXT + 1,
		LIST_POS				= LIST_CURPOS + 1,
		LIST_RANDOM				= LIST_POS + 1,
		LIST_REPEAT				= LIST_RANDOM + 1,
		LIST_RM_ON_ERR			= LIST_REPEAT + 1,
		LIST_RM_ON_NEXT			= LIST_RM_ON_ERR + 1,
		OP_DATA_DIR				= LIST_RM_ON_NEXT + 1,
		OP_FILE_DELETE			= OP_DATA_DIR + 1,
		OP_PLIST_SAVE_DIR		= OP_FILE_DELETE + 1,
		OP_QUICK_MOVE_DIR		= OP_PLIST_SAVE_DIR + 1,
		OP_TRASH_DIR_REL		= OP_QUICK_MOVE_DIR + 1,
		PLAY_AUTO_SKIP			= OP_TRASH_DIR_REL + 1,
		PLAY_AUTO_SKIP_TAIL		= PLAY_AUTO_SKIP + 1,
		PLAY_AUTO_STOP			= PLAY_AUTO_SKIP_TAIL + 1,
		PLAY_NO_TAGS			= PLAY_AUTO_STOP + 1,
		REC_BITRATE				= PLAY_NO_TAGS + 1,
		REC_BUF_LEN				= REC_BITRATE + 1,
		REC_CHANNELS			= REC_BUF_LEN + 1,
		REC_DANORM				= REC_CHANNELS + 1,
		REC_ENC					= REC_DANORM + 1,
		REC_EXCLUSIVE			= REC_ENC + 1,
		REC_GAIN				= REC_EXCLUSIVE + 1,
		REC_PATH				= REC_GAIN + 1,
		REC_RATE				= REC_PATH + 1,
		REC_UNTIL				= REC_RATE + 1,
		UI_CURPATH				= REC_UNTIL + 1,
		UI_FILTER_HIDE			= UI_CURPATH + 1,
		UI_INFO_IN_TITLE		= UI_FILTER_HIDE + 1,
		UI_RECORD_HIDE			= UI_INFO_IN_TITLE + 1,
		UI_STATE_HIDE			= UI_RECORD_HIDE + 1,
		UI_SVC_NOTFN_DISABLE	= UI_STATE_HIDE + 1,
		UI_THEME				= UI_SVC_NOTFN_DISABLE+1
		;
	native Entry[] confRead(String filepath);
	native boolean confWrite(String filepath, byte[] data);
}

class UtilNative {
	UtilNative(Phiola phi) {}

	String[] storage_paths;
	native String trash(String trash_dir, String filepath);
	native String fileMove(String filepath, String target_dir);
}
