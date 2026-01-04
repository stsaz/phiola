/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

class Phiola {
	Phiola(String libdir, Object asset_mgr) {
		System.load(String.format("%s/libphiola.so", libdir));
		init(libdir, asset_mgr);
	}
	private native void init(String libdir, Object asset_mgr);
	native void destroy();
	private static boolean lib_load(String filename) {
		System.load(filename);
		return true;
	}

	native String version();
	native void setConfig(String codepage, boolean deprecated_mods);
	native void setDebug(boolean enable);

	static class Meta {
		int queue_pos;
		long length_msec;
		String url, artist, title, album, date, info;
		String[] meta;
		static final int N_RESERVED = 5; // URL, SIZE, MTIME, LENGTH, FORMAT
	}
	static final int
		PCS_STOP = 1,
		PCS_AUTOSTOP = 2;
	interface PlayObserver {
		void on_create(Meta meta);
		void on_close(int status);
		void on_update(long pos_msec);
	}
	native void playObserverSet(PlayObserver obs, int flags);

	static final int
		PC_PAUSE_TOGGLE = 1,
		PC_STOP = 2,
		PC_AUTO_SKIP_HEAD = 3,
		PC_AUTO_SKIP_TAIL = 4,
		PC_AUTO_STOP = 5,
		PC_SEEK = 6;
	native void playCmd(int cmd, long val);

	static final int
		AF_AAC_LC = 0,
		AF_AAC_HE = 1,
		AF_AAC_HE2 = 2,
		AF_FLAC = 3,
		AF_OPUS = 4,
		AF_OPUS_VOICE = 5,
		AF_MP3 = 6;

	static class ConvertParams {
		ConvertParams() {
			out_name = "";
			from_msec = "";
			to_msec = "";
			tags = "";
			trash_dir_rel = "";
			q_pos = -1;
		}

		int format; // AF_*

		static final int
			COF_DATE_PRESERVE = 1,
			COF_OVERWRITE = 2,
			COF_COPY = 4,
			COF_ADD = 8; // add item to playlist `q_add_remove`
		int flags;

		String out_name;
		String from_msec, to_msec;
		String tags;
		int sample_format;
		int sample_rate;
		int aac_quality;
		int opus_quality;
		int mp3_quality;
		long q_add_remove;
		int q_pos;
		String trash_dir_rel;
	}

	// RECORD

	static class RecordParams {
		int format; // AF_*

		int channels;
		int sample_rate;

		static final int
			RECF_EXCLUSIVE = 1,
			RECF_POWER_SAVE = 2,
			RECF_UNPROCESSED = 4,
			RECF_DANORM = 8;
		int flags;

		int buf_len_msec;
		int gain_db100;
		int quality;
		int until_sec;
	}
	interface RecordCallback {
		void on_finish(int code, String filename);
	}
	native long recStart(String oname, RecordParams conf, RecordCallback cb);
	static final int
		RECL_STOP = 1,
		RECL_PAUSE = 2,
		RECL_RESUME = 3;
	native String recCtrl(long trk, int cmd);

	// TAGS EDIT

	static final int
		TE_CLEAR = 1,
		TE_PRESERVE_DATE = 2;
	native int tagsEdit(String filename, String[] tags, int flags);

	// track queue

	interface QueueCallback {
		void on_change(long q, int flags, int pos);
		void on_complete(int operation, int status);
	}
	native void quSetCallback(QueueCallback cb);

	static final int
		QUNF_CONVERSION = 1;
	native long quNew(int flags);
	native void quDestroy(long q);

	/** Move the list to a new position. */
	native void quMove(int from, int to);

	/** Copy entries from another queue (asynchronous).
	pos: Source entry index
		-1: Copy all entries */
	native void quDup(long q, long q_src, int pos);

	static final int QUADD_RECURSE = 1;
	native void quAdd(long q, String[] urls, int flags);

	native String quEntry(long q, int i);

	static final int
		QUCOM_CLEAR = 1,
		QUCOM_REMOVE_I = 2,
		QUCOM_COUNT = 3,
		/** Convert track index in the currently visible (filtered) list
		 to the index within its parent (not filtered) list */
		QUCOM_INDEX = 4,
		QUCOM_SORT = 5,
			QU_SORT_FILENAME = 0,
			QU_SORT_FILESIZE = 1,
			QU_SORT_FILEDATE = 2,
			QU_SORT_RANDOM = 3,
			QU_SORT_TAG_ARTIST = 4,
			QU_SORT_TAG_DATE = 5,
		QUCOM_REMOVE_NON_EXISTING = 6,
		QUCOM_PLAY = 7,
		QUCOM_PLAY_NEXT = 8,
		QUCOM_PLAY_PREV = 9,
		QUCOM_META_READ = 10,
		QUCOM_CONV_CANCEL = 13,

		/** Update current status of all entries.
		Return 0 when conversion is complete. */
		QUCOM_CONV_UPDATE = 14;
	native int quCmd(long q, int cmd, int i);

	static final int
		QC_REPEAT = 1,
		QC_RANDOM = 2,
		QC_REMOVE_ON_ERROR = 4,
		QC_AUTO_NORM = 0x10,
		QC_RG_NORM = 0x20;
	native void quConf(int mask, int val);

	native Meta quMeta(long q, int i);

	/** Move all files in the list to the specified directory.
	Note: the URLs are NOT updated.
	Return N of files moved;
		<0 if some files were not moved. */
	native int quMoveAll(long q, String dst_dir);

	static final int
		QR_MOVE = 0, // move to the specified directory
		QR_RENAME = 1; // rename file
	/** Move file */
	native int quRename(long q, int pos, String target, int flags);

	static final int
		QUFILTER_URL = 1,
		QUFILTER_META = 2;
	native long quFilter(long q, String filter, int flags);

	native String quConvertBegin(long q, ConvertParams conf);

	native String[] quDisplayLine(long q, int i, int n);

	/** Load playlist from a file on disk */
	native int quLoad(long q, String filepath);

	native void quSave(long q, String filepath);
}
