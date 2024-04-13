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
		int length_msec;
		String url, artist, title, album, date, info;
		String[] meta;
	}
	interface MetaCallback {
		void on_finish(Meta meta);
	}
	native int meta(long q, int list_item, String filepath, MetaCallback cb);

	static final int
		AF_AAC_LC = 0,
		AF_AAC_HE = 1,
		AF_AAC_HE2 = 2,
		AF_FLAC = 3,
		AF_OPUS = 4,
		AF_OPUS_VOICE = 5;

	static class ConvertParams {
		ConvertParams() {
			from_msec = "";
			to_msec = "";
		}

		int format;

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
	native String recStop(long trk);

	// track queue

	interface QueueCallback {
		void on_change(long q, int flags, int pos);
	}
	native void quSetCallback(QueueCallback cb);

	native long quNew();
	native void quDestroy(long q);

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
			QU_SORT_RANDOM = 1,
		QUCOM_REMOVE_NON_EXISTING = 6;
	native int quCmd(long q, int cmd, int i);

	native Meta quMeta(long q, int i);

	static final int QUFILTER_URL = 1;
	static final int QUFILTER_META = 2;
	native long quFilter(long q, String filter, int flags);

	/** Load playlist from a file on disk */
	native int quLoad(long q, String filepath);

	native boolean quSave(long q, String filepath);
}
