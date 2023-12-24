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

	native String[] confRead(String filepath);
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
		static final int REC_AACLC = 0;
		static final int REC_AACHE = 1;
		static final int REC_AACHE2 = 2;
		static final int REC_FLAC = 3;
		static final int REC_OPUS = 4;
		static final int REC_OPUS_VOICE = 5;
		int format;

		static final int RECF_EXCLUSIVE = 1;
		static final int RECF_POWER_SAVE = 2;
		static final int RECF_DANORM = 4;
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
