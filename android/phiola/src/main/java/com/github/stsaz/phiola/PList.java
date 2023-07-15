/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

class QueueItemInfo {
	QueueItemInfo() {
		url = "";
		artist = "";
		title = "";
	}
	String url, artist, title;
	int length_sec;
}

class PList {
	private static final String TAG = "phiola.Queue";
	Core core;
	int qi;
	long[] q;
	int curpos = -1;
	boolean[] modified;

	PList(Core core) {
		this.core = core;
		q = new long[2];
		q[0] = core.phiola.quNew();
		q[1] = core.phiola.quNew();
		modified = new boolean[2];
	}

	void destroy() {
		core.phiola.quDestroy(q[0]);
		core.phiola.quDestroy(q[1]);
		q[0] = 0;
		q[1] = 0;
	}

	long sel() {
		return q[qi];
	}

	void add1(String url) {
		String[] urls = new String[1];
		urls[0] = url;
		core.phiola.quAdd(sel(), urls, 0);
		modified[qi] = true;
	}

	void add_r(String[] urls) {
		core.phiola.quAdd(sel(), urls, Phiola.QUADD_RECURSE);
		modified[qi] = true;
	}

	void add(String[] urls) {
		core.phiola.quAdd(sel(), urls, 0);
		modified[qi] = true;
	}

	void iremove(int i, int ie) {
		core.phiola.quCmd(q[i], Phiola.QUCOM_REMOVE_I, ie);
		modified[i] = true;
		if (ie <= curpos)
			curpos--;
	}

	void clear() {
		core.phiola.quCmd(sel(), Phiola.QUCOM_CLEAR, 0);
		modified[qi] = true;
		curpos = -1;
	}

	String get(int i) {
		return core.phiola.quEntry(sel(), i);
	}

	QueueItemInfo getInfo(int i) {
		Phiola.Meta m = core.phiola.quMeta(sel(), i);
		QueueItemInfo qi = new QueueItemInfo();
		qi.url = m.url;
		qi.artist = m.artist;
		qi.title = m.title;
		qi.length_sec = m.length_msec / 1000;
		core.dbglog(TAG, "getInfo: %s '%s' '%s'", qi.url, qi.artist, qi.title);
		return qi;
	}

	int size() {
		return core.phiola.quCmd(sel(), Phiola.QUCOM_COUNT, 0);
	}

	void filter(String filter) {
		core.phiola.quFilter(sel(), filter, Phiola.QUFILTER_URL);
	}

	/** Load playlist from a file on disk */
	void iload_file(int i, String fn) {
		core.phiola.quLoad(q[i], fn);
	}

	/** Save playlist to a file */
	int isave(int i, String fn) {
		if (!core.phiola.quSave(q[i], fn))
			return -1;
		modified[i] = false;
		return 0;
	}

	int save(String fn) {
		if (0 != isave(qi, fn))
			return -1;
		core.dbglog(TAG, "saved %d items to %s", size(), fn);
		return 0;
	}
}
