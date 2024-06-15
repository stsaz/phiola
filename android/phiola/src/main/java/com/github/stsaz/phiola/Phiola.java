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

	native String version();
	native void setCodepage(String codepage);

	static class Meta {
		int queue_pos;
		long length_msec;
		String url, artist, title, album, date, info;
		String[] meta;
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
		AF_OPUS_VOICE = 5;

	static class ConvertParams {
		ConvertParams() {
			out_name = "";
			from_msec = "";
			to_msec = "";
			trash_dir_rel = "";
			q_pos = -1;
		}

		int format;

		static final int
			F_DATE_PRESERVE = 1,
			F_OVERWRITE = 2,
			F_COPY = 4;
		int flags;

		String out_name;
		String from_msec, to_msec;
		int sample_rate;
		int aac_quality;
		int opus_quality;
		int vorbis_quality;
		long q_add_remove;
		int q_pos;
		String trash_dir_rel;
	}

	static class RecordParams {
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
	static final int
		RECL_STOP = 1,
		RECL_PAUSE = 2,
		RECL_RESUME = 3;
	native String recCtrl(long trk, int cmd);

	// track queue

	interface QueueCallback {
		void on_change(long q, int flags, int pos);
	}
	native void quSetCallback(QueueCallback cb);

	static final int
		QUNF_CONVERSION = 1;
	native long quNew(int flags);
	native void quDestroy(long q);

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
		QUCOM_REMOVE_NON_EXISTING = 6,
		QUCOM_PLAY = 7,
		QUCOM_PLAY_NEXT = 8,
		QUCOM_PLAY_PREV = 9,
		QUCOM_REPEAT = 10,
		QUCOM_RANDOM = 11,
		QUCOM_REMOVE_ON_ERROR = 12;
	native int quCmd(long q, int cmd, int i);

	native Meta quMeta(long q, int i);

	static final int
		QUFILTER_URL = 1,
		QUFILTER_META = 2;
	native long quFilter(long q, String filter, int flags);

	native String quConvertBegin(long q, ConvertParams conf);

	/** Update current status of all entries. */
	native int quConvertUpdate(long q);

	native void quConvertInterrupt();

	native String quDisplayLine(long q, int i);

	/** Load playlist from a file on disk */
	native int quLoad(long q, String filepath);

	native boolean quSave(long q, String filepath);
}
