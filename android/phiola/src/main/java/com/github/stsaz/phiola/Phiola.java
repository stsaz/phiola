/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

class Phiola {
	Phiola() {
		length_msec = 0;
		artist = "";
		title = "";
		info = "";
		url = "";
		init();
	}
	private native void init();
	native void destroy();

	native String[] confRead(String filepath);
	native boolean confWrite(String filepath, byte[] data);

	native void setCodepage(String codepage);

	interface Callback {
		void on_finish();
	}

	long length_msec;
	String url, artist, title, info;
	String[] meta_data;
	native int meta(int list_item, String filepath, Callback cb);

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
	}
	interface ConvertCallback {
		void on_finish(String result);
	}
	native int convert(String iname, String oname, ConvertParams conf, ConvertCallback cb);

	static final int REC_AACLC = 0;
	static final int REC_AACHE = 1;
	static final int REC_AACHE2 = 2;
	static final int REC_FLAC = 3;
	static final int RECF_EXCLUSIVE = 0x10;
	static final int RECF_POWER_SAVE = 0x20;
	native long recStart(String oname, int buf_len_msec, int gain_db100, int fmt, int q, int until_sec, int flags, Callback cb);
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
	static final int QUCOM_META = 4; // set url, length_msec, artist, title
	native int quCmd(long q, int cmd, int i);

	static final int QUFILTER_URL = 1;
	static final int QUFILTER_META = 2;
	native int quFilter(long q, String filter, int flags);

	native int quLoad(long q, String filepath);
	native boolean quSave(long q, String filepath);

	String[] storage_paths;
	native String trash(String trash_dir, String filepath);
	native String fileMove(String filepath, String target_dir);

	static {
		System.loadLibrary("phiola");
	}
}
