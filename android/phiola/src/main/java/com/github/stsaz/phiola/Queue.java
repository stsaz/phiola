/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import java.io.File;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;

class PhiolaQueue {
	private Phiola phi;
	long q;
	boolean modified;
	boolean conversion;

	PhiolaQueue(Phiola _phi, long _q) {
		phi = _phi;
		q = _q;
	}

	PhiolaQueue(Phiola _phi, int flags, int dummy) {
		phi = _phi;
		q = phi.quNew(flags);
		conversion = ((flags & Phiola.QUNF_CONVERSION) != 0);
	}

	void destroy() { phi.quDestroy(q); }

	void dup(long q_src, int pos) {
		phi.quDup(q, q_src, pos);
		modified = true;
	}

	void add(String url, int flags) {
		if (conversion) return;

		String[] urls = new String[1];
		urls[0] = url;
		phi.quAdd(q, urls, flags);
		modified = true;
	}

	void add_many(String[] urls, int flags) {
		if (conversion) return;

		phi.quAdd(q, urls, flags);
		modified = true;
	}

	void remove(int i) {
		if (conversion) return;

		phi.quCmd(q, Phiola.QUCOM_REMOVE_I, i);
		modified = true;
	}

	void remove_non_existing() {
		if (conversion) return;

		phi.quCmd(q, Phiola.QUCOM_REMOVE_NON_EXISTING, 0);
		modified = true;
	}

	void clear() {
		if (conversion) return;

		phi.quCmd(q, Phiola.QUCOM_CLEAR, 0);
		modified = true;
	}

	int count() { return phi.quCmd(q, Phiola.QUCOM_COUNT, 0); }

	void sort(int flags) {
		if (conversion) return;

		phi.quCmd(q, Phiola.QUCOM_SORT, flags);
	}

	PhiolaQueue filter(String filter, int flags) {
		return new PhiolaQueue(phi, phi.quFilter(q, filter, flags));
	}

	boolean save(String filepath) {
		boolean b = phi.quSave(q, filepath);
		if (b)
			modified = false;
		return b;
	}
}

interface QueueNotify {
	static final int
		UPDATE = 0,
		ADDED = 1,
		REMOVED = 2,
		CONVERT_COMPLETE = 3;
	/** Called when the queue is modified. */
	void on_change(int how, int pos);
}

class Queue {
	private static final String TAG = "phiola.Queue";
	private Core core;
	private Phiola phi;
	private int trk_idx = -1; // Active track index
	private boolean active;
	private ArrayList<QueueNotify> nfy;

	private ArrayList<PhiolaQueue> queues;
	private PhiolaQueue q_filtered;
	private int i_active; // Active queue index
	private int i_selected; // Currently selected queue index
	private int i_conversion = -1;
	private int curpos = -1; // Last active track index
	private int filter_len;

	private boolean converting;
	private Timer convert_update_timer;

	static final int
		F_REPEAT = 1,
		F_RANDOM = 2,
		F_MOVE_ON_NEXT = 4,
		F_RM_ON_NEXT = 8,
		F_RM_ON_ERR = 0x10;
	private int flags;
	void flags_set1(int mask, boolean val) {
		int i = 0;
		if (val)
			i = mask;
		flags_set(mask, i);
	}
	void flags_set(int mask, int val) {
		if ((mask & F_REPEAT) != 0 && (val & F_REPEAT) != (this.flags & F_REPEAT))
			phi.quCmd(-1, Phiola.QUCOM_REPEAT, val & F_REPEAT);

		if ((mask & F_RANDOM) != 0 && (val & F_RANDOM) != (this.flags & F_RANDOM))
			phi.quCmd(-1, Phiola.QUCOM_RANDOM, val & F_RANDOM);

		if ((mask & F_RM_ON_ERR) != 0 && (val & F_RM_ON_ERR) != (this.flags & F_RM_ON_ERR))
			phi.quCmd(-1, Phiola.QUCOM_REMOVE_ON_ERROR, val & F_RM_ON_ERR);

		this.flags &= ~mask;
		this.flags |= val;
	}
	boolean flags_test(int mask) {
		return (this.flags & mask) != 0;
	}

	int		auto_stop_value_min;
	boolean	auto_stop_active;
	int auto_stop_toggle() {
		auto_stop_active = !auto_stop_active;
		int interval_msec = 0;
		if (auto_stop_active)
			interval_msec = auto_stop_value_min * 60*1000;
		phi.playCmd(Phiola.PC_AUTO_STOP, interval_msec);
		if (auto_stop_active)
			return auto_stop_value_min;
		return 0;
	}

	Queue(Core core) {
		this.core = core;
		phi = core.phiola;
		core.track.observer_add(new PlaybackObserver() {
				public int open(TrackHandle t) {
					play_on_open(t);
					return 0;
				}
				public void close(TrackHandle t) { play_on_close(t); }
			});
		queues = new ArrayList<>();
		nfy = new ArrayList<>();
	}

	private void on_change(long q, int flags, int pos) {
		core.tq.post(() -> {
				if (queues.size() == 0)
					return;

				switch (flags) {
				case 'r':
					if (q == queues.get(i_active).q) {
						if (pos < curpos)
							curpos--;

						if (pos == trk_idx)
							trk_idx = -1;
						else if (pos < trk_idx)
							trk_idx--;
					}
					break;
				}

				if (i_selected < 0 // after Queue.close()
					|| q != queues.get(i_selected).q)
					return;

				switch (flags) {
				case 'u':
					nfy_all(QueueNotify.UPDATE, -1);  break;

				case 'c':
					nfy_all(QueueNotify.REMOVED, -1);  break;

				case 'r':
					nfy_all(QueueNotify.REMOVED, pos);  break;
				}
			});
	}

	int new_list() {
		filter_close();
		queues.add(new PhiolaQueue(core.phiola, 0, 0));
		return queues.size() - 1;
	}

	int close_current_list() {
		if (i_selected == i_conversion && converting)
			return E_BUSY; // can't close the conversion list while the conversion is in progress

		int n_playlists = queues.size();
		if (i_conversion >= 0)
			n_playlists--;
		if (n_playlists == 1 && i_selected != i_conversion) {
			clear();
			return 0;
		}

		filter_close();
		queues.get(i_selected).destroy();
		queues.remove(i_selected);
		if (i_active == i_selected)
			i_active = 0;

		if (i_selected == i_conversion)
			i_conversion = -1;
		else if (i_selected < i_conversion)
			i_conversion--;

		i_selected--;
		if (i_selected < 0)
			i_selected = 0;

		// As positions of all next lists have just been changed, we must rewrite the files on disk accordingly
		int i = 0;
		for (PhiolaQueue q : queues) {
			if (i++ >= i_selected)
				q.modified = true;
		}
		return 0;
	}

	void close() {
		filter_close();
		for (PhiolaQueue q : queues) {
			q.destroy();
		}
		queues.clear();
		i_selected = -1;
		i_conversion = -1;
	}

	private String list_file_name(int i) {
		return String.format("%s/list%d.m3uz", core.setts.pub_data_dir, i+1);
	}

	void load() {
		int i;
		for (i = 0;;  i++) {
			String fn = list_file_name(i);

			if (!new File(fn).exists())
				break;

			new_list();
			phi.quLoad(queues.get(i).q, fn);
		}

		if (i == 0) {
			new_list();
			i = 1;
		}

		if (i_active >= i)
			i_active = 0;
		if (i_selected >= i)
			i_selected = 0;

		if (flags_test(F_REPEAT))
			phi.quCmd(-1, Phiola.QUCOM_REPEAT, 1);
		if (flags_test(F_RANDOM))
			phi.quCmd(-1, Phiola.QUCOM_RANDOM, 1);
		if (flags_test(F_RM_ON_ERR))
			phi.quCmd(-1, Phiola.QUCOM_REMOVE_ON_ERROR, 1);

		core.phiola.quSetCallback(this::on_change);
	}

	void saveconf() {
		core.dir_make(core.setts.pub_data_dir);
		int i = 0;
		for (PhiolaQueue q : queues) {
			if (q.conversion)
				continue;
			if (q.modified)
				q.save(list_file_name(i));
			i++;
		}

		core.file_delete(list_file_name(i));
	}

	/** Save playlist to a file */
	boolean save(String fn) {
		if (!q_selected().save(fn))
			return false;
		core.dbglog(TAG, "saved %s", fn);
		return true;
	}

	/** Get next list index (round-robin) */
	private int index_next(int qi) {
		int ni = qi + 1;
		if (ni == queues.size())
			ni = 0;
		return ni;
	}

	/** Change to next playlist */
	int next_list_select() {
		filter("");
		i_selected = index_next(i_selected);
		return i_selected;
	}

	boolean conversion_list(int i) { return i == i_conversion; }

	void switch_list(int i) { i_selected = i; }

	int current_list_index() { return i_selected; }
	long current_list_id() { return queues.get(i_selected).q; }

	/** Add currently playing track to next list.
	Return the modified list index. */
	int next_list_add_cur() {
		if (queues.size() == 1) return -1;

		String url = core.track.cur_url();
		if (url.isEmpty())
			return -1;

		int ni = index_next(i_active);
		filter_close();
		queues.get(ni).add(url, 0);
		return ni;
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

	/** Play track at cursor */
	void playcur() {
		int pos = curpos;
		if (pos < 0)
			pos = 0;
		_play(i_active, pos);
	}

	private void _play(int iq, int it) {
		core.dbglog(TAG, "play: %d %d", iq, it);
		if (active)
			core.track.stop();

		if (it < 0)
			return;

		i_active = iq;
		core.phiola.quCmd(queues.get(iq).q, Phiola.QUCOM_PLAY, it);
	}

	/** Play track at the specified position */
	void play(int index) {
		_play(i_selected, index);
	}

	void visiblelist_play(int i) {
		if (i_selected == i_conversion)
			return; // ignore click on entry in Conversion list

		_play(i_selected, phi.quCmd(q_visible().q, Phiola.QUCOM_INDEX, i));
	}

	private int next_playlist(int i) {
		int n = i + 1;
		if (n == queues.size())
			n = 0;
		if (queues.get(n).conversion)
			n++;
		if (n == queues.size())
			n = 0;
		if (n == i)
			return -1;
		return n;
	}

	/** Next track by user command */
	void order_next() {
		if (flags_test(F_MOVE_ON_NEXT) && trk_idx >= 0) {
			String url = phi.quEntry(queues.get(i_active).q, trk_idx);
			queues.get(i_active).remove(trk_idx);
			int npl = next_playlist(i_active);
			if (npl >= 0)
				queues.get(npl).add(url, 0);

		} else if (flags_test(F_RM_ON_NEXT) && trk_idx >= 0) {
			queues.get(i_active).remove(trk_idx);
		}

		phi.quCmd(queues.get(i_active).q, Phiola.QUCOM_PLAY_NEXT, -1);
	}

	/** Previous track by user command */
	void order_prev() {
		phi.quCmd(queues.get(i_active).q, Phiola.QUCOM_PLAY_PREV, -1);
	}

	private void play_on_open(TrackHandle t) {
		trk_idx = t.pmeta.queue_pos;
		curpos = trk_idx;
		active = true;
		queues.get(i_active).modified = true;
		nfy_all(QueueNotify.UPDATE, trk_idx); // redraw item to display artist-title info
	}

	/** Called after a track has been finished. */
	private void play_on_close(TrackHandle t) {
		active = false;
		trk_idx = -1;
		if ((t.close_status & Phiola.PCS_AUTOSTOP) != 0)
			auto_stop_active = false;
	}

	String get(int i) { return phi.quEntry(queues.get(i_selected).q, i); }

	String display_line(int i) {
		return phi.quDisplayLine(q_visible().q, i);
	}

	void remove(int pos) {
		core.dbglog(TAG, "remove: %d:%d", i_active, pos);
		filter_close();
		queues.get(i_active).remove(pos);
	}

	/** Clear playlist */
	void clear() {
		core.dbglog(TAG, "clear");
		filter_close();
		queues.get(i_selected).clear();
		curpos = -1;
		trk_idx = -1;
	}

	void rm_non_existing() {
		queues.get(i_selected).remove_non_existing();
	}

	/** Get tracks number in the currently selected (not filtered) list */
	int count() { return queues.get(i_selected).count(); }

	int visiblelist_itemcount() { return q_visible().count(); }

	static final int ADD_RECURSE = 1;
	static final int ADD = 2;

	void addmany(String[] urls, int flags) {
		int pos = count();
		filter_close();
		int f = 0;
		if (flags == ADD_RECURSE)
			f = Phiola.QUADD_RECURSE;
		queues.get(i_selected).add_many(urls, f);
		nfy_all(QueueNotify.ADDED, pos);
	}

	/** Add an entry */
	void add(String url) {
		core.dbglog(TAG, "add: %s", url);
		int pos = count();
		filter_close();
		queues.get(i_selected).add(url, 0);
		nfy_all(QueueNotify.ADDED, pos);
	}

	String conf_write() {
		return String.format(
			"list_curpos %d\n"
			+ "list_active %d\n"
			+ "list_random %d\n"
			+ "list_repeat %d\n"
			+ "list_add_rm_on_next %d\n"
			+ "list_rm_on_next %d\n"
			+ "list_rm_on_err %d\n"
			+ "play_auto_stop %d\n"
			, curpos
			, i_active
			, core.bool_to_int(flags_test(F_RANDOM))
			, core.bool_to_int(flags_test(F_REPEAT))
			, core.bool_to_int(flags_test(F_MOVE_ON_NEXT))
			, core.bool_to_int(flags_test(F_RM_ON_NEXT))
			, core.bool_to_int(flags_test(F_RM_ON_ERR))
			, auto_stop_value_min
			);
	}

	void conf_load(Conf.Entry[] kv) {
		curpos = kv[Conf.LIST_CURPOS].number;
		i_active = kv[Conf.LIST_ACTIVE].number;
		i_selected = i_active;

		int f = 0;
		if (kv[Conf.LIST_RANDOM].enabled) f |= F_RANDOM;
		if (kv[Conf.LIST_REPEAT].enabled) f |= F_REPEAT;
		if (kv[Conf.LIST_ADD_RM_ON_NEXT].enabled) f |= F_MOVE_ON_NEXT;
		if (kv[Conf.LIST_RM_ON_NEXT].enabled) f |= F_RM_ON_NEXT;
		if (kv[Conf.LIST_RM_ON_ERR].enabled) f |= F_RM_ON_ERR;
		this.flags = f;

		auto_stop_value_min = kv[Conf.PLAY_AUTO_STOP].number;
	}

	void conf_normalize() {
		if (auto_stop_value_min == 0)
			auto_stop_value_min = 60;
	}

	/** Get currently playing track index */
	int active_track_pos() {
		if (i_selected != i_active)
			return -1;
		return trk_idx;
	}

	String[] meta(int i) {
		if (i == -1)
			i = trk_idx;
		Phiola.Meta m = phi.quMeta(queues.get(i_active).q, i);
		if (m == null)
			return null;
		return m.meta;
	}

	void sort(int flags) {
		if (q_filtered != null) return;

		q_selected().sort(flags);
	}

	long q_active_id() { return queues.get(i_active).q; }

	/** Currently selected (not filtered) list */
	private PhiolaQueue q_selected() { return queues.get(i_selected); }

	/** Currently visible (filtered) list */
	private PhiolaQueue q_visible() {
		if (q_filtered != null)
			return q_filtered;
		return queues.get(i_selected);
	}

	private void filter_close() {
		if (q_filtered != null) {
			q_filtered.destroy();
			q_filtered = null;
		}
	}

	/** Create filtered list */
	void filter(String filter) {
		PhiolaQueue newqf = null;
		if (!filter.isEmpty()) {
			PhiolaQueue qcur = queues.get(i_selected);
			if (q_filtered != null && filter.length() > filter_len)
				qcur = q_filtered;
			int f = Phiola.QUFILTER_URL | Phiola.QUFILTER_META;
			newqf = qcur.filter(filter, f);
		}

		filter_len = filter.length();

		filter_close();
		q_filtered = newqf;
	}

	static final int
		E_EXIST = -1,
		E_NOENT = -2,
		E_BUSY = -3;

	static final int
		CONV_CUR_LIST = 0,
		CONV_CUR_FILE = 1;

	/** Create conversion queue and add tracks to it from the currently selected queue. */
	int convert_add(int flags) {
		if (converting)
			return E_BUSY;
		if (i_conversion >= 0)
			return E_EXIST; // conversion list exists already

		int i = -1;
		long q_src = 0;
		if (flags == CONV_CUR_LIST) {
			if (queues.get(i_selected).count() == 0)
				return E_NOENT; // empty playlist
			q_src = queues.get(i_selected).q;
		} else if (flags == CONV_CUR_FILE) {
			if (trk_idx < 0)
				return E_NOENT; // no active track
			q_src = queues.get(i_active).q;
			i = trk_idx;
		}

		filter_close();
		queues.add(new PhiolaQueue(core.phiola, Phiola.QUNF_CONVERSION, 0));
		i_conversion = queues.size() - 1;

		queues.get(i_conversion).dup(q_src, i);
		i_selected = i_conversion;
		return i_conversion;
	}

	/** Begin conversion and periodically send redraw signals to GUI. */
	String convert_begin(Phiola.ConvertParams conf) {
		String r = core.phiola.quConvertBegin(queues.get(i_conversion).q, conf);
		if (!r.isEmpty())
			return r;

		converting = true;
		convert_update_timer = new Timer();
		convert_update_timer.schedule(new TimerTask() {
				public void run() {
					core.dbglog(TAG, "convert_update_timer fired");
					core.tq.post(() -> convert_update());
				}
			}, 500, 500);
		return null;
	}

	private void convert_update() {
		if (0 == core.phiola.quConvertUpdate(queues.get(i_conversion).q)) {
			converting = false;
			convert_update_timer.cancel();
			convert_update_timer = null;
			nfy_all(QueueNotify.CONVERT_COMPLETE, 0);
		}
		if (i_selected == i_conversion)
			nfy_all(QueueNotify.UPDATE, -1);
	}

	void convert_cancel() {
		core.phiola.quConvertInterrupt();
	}
}
