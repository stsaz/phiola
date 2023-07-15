/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Handler;
import android.os.Looper;

import java.util.ArrayList;
import java.util.Date;
import java.util.Random;

interface QueueNotify {
	static final int UPDATE = 0;
	static final int ADDED = 1;
	static final int REMOVED = 2;
	/**
	 * Called when the queue is modified.
	 */
	void on_change(int how, int pos);
}

class Queue {
	private static final String TAG = "phiola.Queue";
	private Core core;
	private Track track;
	private int trk_q = -1;
	private int trk_idx = -1;
	private ArrayList<QueueNotify> nfy;

	private PList pl;
	private boolean repeat;
	private boolean random;
	private boolean active;
	private Random rnd;
	boolean random_split;
	int autoskip_msec;
	private boolean b_order_next;
	private Handler mloop;

	Queue(Core core) {
		this.core = core;
		track = core.track();
		track.filter_add(new Filter() {
			public int open(TrackHandle t) {
				active = true;
				b_order_next = false;
				if (autoskip_msec != 0)
					t.seek_msec = autoskip_msec;

				if (!core.setts.no_tags) {
					pl.modified[0] = true;
					pl.modified[1] = true;
					nfy_all(QueueNotify.UPDATE, trk_idx); // redraw item to display artist-title info
				}

				return 0;
			}

			public void close(TrackHandle t) {
				on_close(t);
			}
		});
		pl = new PList(core);
		nfy = new ArrayList<>();

		mloop = new Handler(Looper.getMainLooper());
	}

	void close() {
		pl.destroy();
	}

	void load() {
		pl.iload_file(0, core.setts.pub_data_dir + "/list1.m3uz");
		pl.iload_file(1, core.setts.pub_data_dir + "/list2.m3uz");
	}

	void saveconf() {
		core.dir_make(core.setts.pub_data_dir);
		if (pl.modified[0])
			pl.isave(0, core.setts.pub_data_dir + "/list1.m3uz");
		if (pl.modified[1])
			pl.isave(1, core.setts.pub_data_dir + "/list2.m3uz");
	}

	/**
	 * Save playlist to a file
	 */
	boolean save(String fn) {
		return 0 == pl.save(fn);
	}

	/** Switch between playlists */
	int switch_list() {
		int i = pl.qi + 1;
		if (i == pl.q.length)
			i = 0;
		pl.qi = i;
		return i;
	}

	long cur_q() {
		return pl.sel();
	}

	/** Add currently playing track to L2 */
	boolean l2_add_cur() {
		if (pl.qi != 0)
			return false;
		String url = track.cur_url();
		if (url.isEmpty())
			return false;
		pl.qi = 1;
		pl.add1(url);
		pl.qi = 0;
		return true;
	}

	void nfy_add(QueueNotify qn) {
		nfy.add(qn);
	}

	void nfy_rm(QueueNotify qn) {
		nfy.remove(qn);
	}

	private void nfy_all(int how, int first_pos) {
		for (QueueNotify qn : nfy) {
			qn.on_change(how, first_pos);
		}
	}

	/**
	 * Play track at cursor
	 */
	void playcur() {
		int pos = pl.curpos;
		if (pos < 0 || pos >= pl.size())
			pos = 0;
		play(pos);
	}

	/**
	 * Play track at the specified position
	 */
	void play(int index) {
		core.dbglog(TAG, "play: %d", index);
		if (active)
			track.stop();

		if (index < 0 || index >= pl.size())
			return;

		trk_q = pl.qi;
		trk_idx = index;
		pl.curpos = index;
		track.start(index, pl.get(index));
	}

	/**
	 * Get random index
	 */
	int next_random() {
		int n = pl.size();
		if (n == 1)
			return 0;
		int i = rnd.nextInt();
		i &= 0x7fffffff;
		if (!random_split)
			i %= n / 2;
		else
			i = n / 2 + (i % (n - (n / 2)));
		random_split = !random_split;
		return i;
	}

	/**
	 * Play next track
	 */
	void next() {
		int i = pl.curpos + 1;
		if (random) {
			if (pl.size() == 0)
				return;
			i = next_random();
		} else if (repeat) {
			if (i == pl.size())
				i = 0;
		}
		play(i);
	}

	/**
	 * Next track by user command
	 */
	void order_next() {
		if (active) {
			b_order_next = true;
			track.stop();
		}
		next();
	}

	/**
	 * Play previous track
	 */
	void prev() {
		play(pl.curpos - 1);
	}

	/**
	 * Called after a track has been finished.
	 */
	private void on_close(TrackHandle t) {
		active = false;
		boolean play_next = !t.stopped;

		if (trk_idx >= 0
			&& ((t.error && core.setts.qu_rm_on_err)
				|| (b_order_next && core.setts.list_rm_on_next))) {
			String url = get(trk_idx);
			if (url.equals(t.url))
				remove(trk_idx);
		}
		trk_idx = -1;

		if (play_next) {
			mloop.post(() -> next());
		}
	}

	/**
	 * Set Random switch
	 */
	void random(boolean val) {
		random = val;
		if (val)
			rnd = new Random(new Date().getTime());
	}

	boolean is_random() {
		return random;
	}

	void repeat(boolean val) {
		repeat = val;
	}

	boolean is_repeat() {
		return repeat;
	}

	String get(int i) {
		return pl.get(i);
	}

	QueueItemInfo getInfo(int i) {
		return pl.getInfo(i);
	}

	void remove(int pos) {
		core.dbglog(TAG, "remove: %d:%d", trk_q, pos);
		pl.iremove(trk_q, pos);
		if (pos == trk_idx)
			trk_idx = -1;
		else if (pos < trk_idx)
			trk_idx--;
		nfy_all(QueueNotify.REMOVED, pos);
	}

	/**
	 * Clear playlist
	 */
	void clear() {
		core.dbglog(TAG, "clear");
		pl.clear();
		trk_idx = -1;
		nfy_all(QueueNotify.REMOVED, -1);
	}

	int count() {
		return pl.size();
	}

	static final int ADD_RECURSE = 1;
	static final int ADD = 2;

	void addmany(String[] urls, int flags) {
		int pos = pl.size();
		if (flags == ADD) {
			pl.add(urls);
		} else {
			pl.add_r(urls);
		}
		nfy_all(QueueNotify.ADDED, pos);
	}

	/**
	 * Add an entry
	 */
	void add(String url) {
		core.dbglog(TAG, "add: %s", url);
		int pos = pl.size();
		pl.add1(url);
		nfy_all(QueueNotify.ADDED, pos);
	}

	String writeconf() {
		StringBuilder s = new StringBuilder();
		if (pl.curpos >= 0)
			s.append(String.format("curpos %d\n", pl.curpos));

		s.append(String.format("random %d\n", core.bool_to_int(random)));
		s.append(String.format("repeat %d\n", core.bool_to_int(repeat)));
		s.append(String.format("autoskip_msec %d\n", autoskip_msec));

		return s.toString();
	}

	int readconf(String k, String v) {
		if (k.equals("curpos")) {
			pl.curpos = core.str_to_uint(v, 0);

		} else if (k.equals("random")) {
			int val = core.str_to_uint(v, 0);
			if (val == 1)
				random(true);

		} else if (k.equals("autoskip_msec")) {
			autoskip_msec = core.str_to_uint(v, 0);

		} else {
			return 1;
		}
		return 0;
	}

	/**
	 * Get currently playing track index
	 */
	int cur() {
		if (pl.qi != trk_q)
			return -1;
		return trk_idx;
	}

	void filter(String filter) {
		pl.filter(filter);
	}
}
