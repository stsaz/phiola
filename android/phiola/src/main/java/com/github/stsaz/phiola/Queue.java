/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Handler;
import android.os.Looper;
import java.io.File;
import java.util.ArrayList;
import java.util.Date;
import java.util.Random;
import java.util.Timer;
import java.util.TimerTask;

class QueueItemInfo {
	QueueItemInfo() {
		url = "";
		artist = "";
		title = "";
	}
	String url, artist, title;
	int length_sec;
}

class PhiolaQueue {
	Phiola phi;
	long q;
	boolean modified;

	PhiolaQueue(Phiola _phi) {
		phi = _phi;
		q = phi.quNew();
	}

	PhiolaQueue(Phiola _phi, long _q) {
		phi = _phi;
		q = _q;
	}

	void destroy() { phi.quDestroy(q); }

	void add(String url, int flags) {
		String[] urls = new String[1];
		urls[0] = url;
		phi.quAdd(q, urls, flags);
		modified = true;
	}

	void add_many(String[] urls, int flags) {
		phi.quAdd(q, urls, flags);
		modified = true;
	}

	String url(int i) { return phi.quEntry(q, i); }

	void remove(int i) {
		phi.quCmd(q, Phiola.QUCOM_REMOVE_I, i);
		modified = true;
	}

	void remove_non_existing() {
		phi.quCmd(q, Phiola.QUCOM_REMOVE_NON_EXISTING, 0);
		modified = true;
	}

	void clear() {
		phi.quCmd(q, Phiola.QUCOM_CLEAR, 0);
		modified = true;
	}

	int count() { return phi.quCmd(q, Phiola.QUCOM_COUNT, 0); }

	int index_real(int i) { return phi.quCmd(q, Phiola.QUCOM_INDEX, i); }

	Phiola.Meta meta(int i) { return phi.quMeta(q, i); }

	void sort(int flags) { phi.quCmd(q, Phiola.QUCOM_SORT, flags); }

	PhiolaQueue filter(String filter, int flags) {
		return new PhiolaQueue(phi, phi.quFilter(q, filter, flags));
	}

	void load(String filepath) { phi.quLoad(q, filepath); }

	boolean save(String filepath) {
		boolean b = phi.quSave(q, filepath);
		if (b)
			modified = false;
		return b;
	}
}

interface QueueNotify {
	static final int UPDATE = 0;
	static final int ADDED = 1;
	static final int REMOVED = 2;
	/** Called when the queue is modified. */
	void on_change(int how, int pos);
}

class Queue {
	private static final String TAG = "phiola.Queue";
	private Core core;
	private Track track;
	private int trk_idx = -1; // Active track index
	private ArrayList<QueueNotify> nfy;

	private ArrayList<PhiolaQueue> queues;
	private PhiolaQueue q_filtered;
	private int q_active; // Active queue index
	private int selected; // Currently selected queue index
	private int curpos = -1; // Last active track index
	private int filter_len;

	private int consecutive_errors;
	private boolean repeat;
	private boolean random;
	private boolean active;
	private Random rnd;
	boolean random_split;
	boolean add_rm_on_next, rm_on_next;
	boolean rm_on_err;
	int autoskip_msec, autoskip_percent;
	int autoskip_tail_msec, autoskip_tail_percent;
	private Handler mloop;

	int				auto_stop_min;
	private Timer	auto_stop_timer;
	private boolean	auto_stop_active;

	Queue(Core core) {
		this.core = core;
		core.phiola.quSetCallback(this::on_change);
		track = core.track();
		track.filter_add(new Filter() {
				public int open(TrackHandle t) {
					on_open(t);
					return 0;
				}

				public void close(TrackHandle t) {
					on_close(t);
				}
			});

		queues = new ArrayList<>();
		queues.add(new PhiolaQueue(core.phiola));

		nfy = new ArrayList<>();
		mloop = new Handler(Looper.getMainLooper());
	}

	private void on_change(long q, int flags, int pos) {
		mloop.post(() -> {
				if (q != queues.get(selected).q)
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
		queues.add(new PhiolaQueue(core.phiola));
		return queues.size() - 1;
	}

	void close_current_list() {
		if (queues.size() == 1) {
			clear();
			return;
		}

		filter_close();
		queues.get(selected).destroy();
		queues.remove(selected);
		selected--;
		if (selected < 0)
			selected = 0;
	}

	void close() {
		filter_close();
		for (PhiolaQueue q : queues) {
			q.destroy();
		}
		queues.clear();
		selected = -1;
	}

	private String list_file_name(int i) {
		return String.format("%s/list%d.m3uz", core.setts.pub_data_dir, i+1);
	}

	void load() {
		for (int i = 0;;  i++) {
			String fn = list_file_name(i);

			if (!new File(fn).exists())
				break;

			if (i != 0)
				queues.add(new PhiolaQueue(core.phiola));

			queues.get(i).load(fn);
		}

		if (q_active >= queues.size())
			q_active = 0;
		if (selected >= queues.size())
			selected = 0;
	}

	void saveconf() {
		core.dir_make(core.setts.pub_data_dir);
		int i = 0;
		for (PhiolaQueue q : queues) {
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
		selected = index_next(selected);
		return selected;
	}

	void switch_list(int i) { selected = i; }

	int current_list_index() { return selected; }

	/** Add currently playing track to next list.
	Return the modified list index. */
	int next_list_add_cur() {
		if (queues.size() == 1) return -1;

		String url = track.cur_url();
		if (url.isEmpty())
			return -1;

		int ni = index_next(q_active);
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
		if (pos < 0 || pos >= queues.get(q_active).count())
			pos = 0;
		_play(q_active, pos);
	}

	private void _play(int iq, int it) {
		core.dbglog(TAG, "play: %d %d", iq, it);
		if (active)
			track.stop();

		if (it < 0 || it >= queues.get(iq).count())
			return;

		q_active = iq;
		trk_idx = it;
		curpos = it;
		track.start(it, queues.get(iq).url(it));
	}

	/** Play track at the specified position */
	void play(int index) {
		_play(selected, index);
	}

	void visiblelist_play(int i) {
		_play(selected, q_visible().index_real(i));
	}

	/** Get random index */
	private int next_random(int n) {
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

	/** Play next or previous track */
	private void play_delta(int delta) {
		int n = queues.get(q_active).count();
		if (n == 0)
			return;

		int i = curpos + delta;
		if (random) {
			i = next_random(n);
		} else if (repeat) {
			if (i >= n)
				i = 0;
			else if (i < 0)
				i = n - 1;
		}
		_play(q_active, i);
	}

	private void next() { play_delta(1); }

	/** Next track by user command */
	void order_next() {
		int delta = 1;
		if (active) {
			if (trk_idx < 0) {
				delta = 0; // user pressed Next after the currently playing track has been removed
			} else if (add_rm_on_next || rm_on_next) {
				if (add_rm_on_next)
					next_list_add_cur();
				remove(trk_idx);
				delta = 0;
			}
		}
		play_delta(delta);
	}

	/** Previous track by user command */
	void order_prev() {
		play_delta(-1);
	}

	private void on_open(TrackHandle t) {
		active = true;

		t.seek_msec = autoskip_msec;
		if (autoskip_percent != 0)
			t.seek_msec = t.time_total_msec * autoskip_percent / 100;

		t.skip_tail_msec = autoskip_tail_msec;
		if (autoskip_tail_percent != 0)
			t.skip_tail_msec = t.time_total_msec * autoskip_tail_percent / 100;

		if (!core.setts.play_no_tags) {
			queues.get(q_active).modified = true;
			nfy_all(QueueNotify.UPDATE, trk_idx); // redraw item to display artist-title info
		}
	}

	/** Called after a track has been finished. */
	private void on_close(TrackHandle t) {
		active = false;
		boolean play_next = !t.stopped;

		if (trk_idx >= 0 && t.error && rm_on_err) {
			String url = queues.get(q_active).url(trk_idx);
			if (url.equals(t.url))
				remove(trk_idx);
		}

		if (auto_stop_active) {
			core.dbglog(TAG, "auto-stop timer: Stopping playback");
			play_next = false;
			auto_stop_toggle();
		}

		if (t.error) {
			this.consecutive_errors++;
			if (this.consecutive_errors >= 20) {
				core.errlog(TAG, "Stopped after %d consecutive errors", this.consecutive_errors);
				this.consecutive_errors = 0;
				play_next = false;
			}
		} else {
			this.consecutive_errors = 0;
		}

		if (play_next) {
			if (trk_idx < 0)
				mloop.post(this::playcur); // play at current position after the track has been removed
			else
				mloop.post(this::next);
		}

		trk_idx = -1;
	}

	/** Set Random switch */
	void random(boolean val) {
		random = val;
		if (val)
			rnd = new Random(new Date().getTime());
	}

	boolean is_random() { return random; }

	void repeat(boolean val) { repeat = val; }

	boolean is_repeat() { return repeat; }

	String get(int i) { return queues.get(selected).url(i); }

	QueueItemInfo info(int i) {
		Phiola.Meta m = q_visible().meta(i);
		QueueItemInfo info = new QueueItemInfo();
		info.url = m.url;
		info.artist = m.artist;
		info.title = m.title;
		info.length_sec = m.length_msec / 1000;
		core.dbglog(TAG, "info: %s '%s' '%s'", info.url, info.artist, info.title);
		return info;
	}

	void remove(int pos) {
		core.dbglog(TAG, "remove: %d:%d", q_active, pos);
		filter_close();
		queues.get(q_active).remove(pos);
		if (pos < curpos)
			curpos--;
		if (pos == trk_idx)
			trk_idx = -1;
		else if (pos < trk_idx)
			trk_idx--;
	}

	/** Clear playlist */
	void clear() {
		core.dbglog(TAG, "clear");
		filter_close();
		queues.get(selected).clear();
		curpos = -1;
		trk_idx = -1;
	}

	void rm_non_existing() {
		queues.get(selected).remove_non_existing();
	}

	/** Get tracks number in the currently selected (not filtered) list */
	int count() { return queues.get(selected).count(); }

	int visiblelist_itemcount() { return q_visible().count(); }

	static final int ADD_RECURSE = 1;
	static final int ADD = 2;

	void addmany(String[] urls, int flags) {
		int pos = count();
		filter_close();
		int f = 0;
		if (flags == ADD_RECURSE)
			f = Phiola.QUADD_RECURSE;
		queues.get(selected).add_many(urls, f);
		nfy_all(QueueNotify.ADDED, pos);
	}

	/** Add an entry */
	void add(String url) {
		core.dbglog(TAG, "add: %s", url);
		int pos = count();
		filter_close();
		queues.get(selected).add(url, 0);
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
			+ "play_auto_skip %s\n"
			+ "play_auto_skip_tail %s\n"
			+ "play_auto_stop %d\n"
			, curpos
			, q_active
			, core.bool_to_int(random)
			, core.bool_to_int(repeat)
			, core.bool_to_int(add_rm_on_next)
			, core.bool_to_int(rm_on_next)
			, core.bool_to_int(rm_on_err)
			, auto_skip_to_str()
			, auto_skip_tail_to_str()
			, auto_stop_min
			);
	}

	int conf_process1(int k, String v) {
		switch (k) {

		case Conf.LIST_CURPOS:
			curpos = core.str_to_uint(v, 0);
			break;

		case Conf.LIST_ACTIVE:
			q_active = core.str_to_uint(v, 0);
			selected = q_active;
			break;

		case Conf.LIST_RANDOM:
			random(core.str_to_bool(v));
			break;

		case Conf.LIST_REPEAT:
			repeat(core.str_to_bool(v));
			break;

		case Conf.LIST_ADD_RM_ON_NEXT:
			add_rm_on_next = core.str_to_bool(v);
			break;

		case Conf.LIST_RM_ON_NEXT:
			rm_on_next = core.str_to_bool(v);
			break;

		case Conf.LIST_RM_ON_ERR:
			rm_on_err = core.str_to_bool(v);
			break;

		case Conf.PLAY_AUTO_SKIP:
			auto_skip(v);
			break;

		case Conf.PLAY_AUTO_SKIP_TAIL:
			auto_skip_tail(v);
			break;

		case Conf.PLAY_AUTO_STOP:
			auto_stop_min = core.str_to_uint(v, 0);
			break;

		default:
			return 1;
		}
		return 0;
	}

	void conf_normalize() {
		if (auto_stop_min == 0)
			auto_stop_min = 60;
	}

	/** Get auto-skip numeric values from string.
	Supports "N%", "N sec" or just N". */
	private int[] auto_skip_convert(String s) {
		int[] a = new int[2];
		if (!s.isEmpty() && s.charAt(s.length() - 1) == '%') {
			a[0] = core.str_to_uint(s.substring(0, s.length() - 1), 0);
			if (a[0] >= 100)
				a[0] = 0;
			a[1] = 0;
		} else {
			if (s.indexOf(" sec") > 0)
				s = s.substring(0, s.length() - 4);
			a[0] = 0;
			a[1] = core.str_to_uint(s, 0) * 1000;
		}
		return a;
	}

	void auto_skip(String s) {
		int[] a = auto_skip_convert(s);
		autoskip_percent = a[0];
		autoskip_msec = a[1];
	}
	String auto_skip_to_str() {
		String s = Integer.toString(autoskip_msec / 1000);
		if (autoskip_percent != 0)
			s = String.format("%d%%", autoskip_percent);
		return s;
	}

	void auto_skip_tail(String s) {
		int[] a = auto_skip_convert(s);
		autoskip_tail_percent = a[0];
		autoskip_tail_msec = a[1];
	}
	String auto_skip_tail_to_str() {
		String s = Integer.toString(autoskip_tail_msec / 1000);
		if (autoskip_tail_percent != 0)
			s = String.format("%d%%", autoskip_tail_percent);
		return s;
	}

	boolean auto_stop_armed() { return (auto_stop_timer != null); }
	boolean auto_stop_toggle() {
		if (auto_stop_timer != null) {
			auto_stop_active = false;
			auto_stop_timer.cancel();
			auto_stop_timer = null;
			return false;
		}

		auto_stop_timer = new Timer();
		auto_stop_timer.schedule(new TimerTask() {
			public void run() {
				core.dbglog(TAG, "auto-stop timer: expired");
				auto_stop_active = true;
			}
		}, auto_stop_min*60*1000);
		return true;
	}

	/** Get currently playing track index */
	int cur() {
		if (selected != q_active)
			return -1;
		return trk_idx;
	}

	void sort(int flags) {
		if (q_filtered != null) return;

		q_selected().sort(flags);
	}

	long q_active_id() { return queues.get(q_active).q; }

	/** Currently selected (not filtered) list */
	private PhiolaQueue q_selected() { return queues.get(selected); }

	/** Currently visible (filtered) list */
	private PhiolaQueue q_visible() {
		if (q_filtered != null)
			return q_filtered;
		return queues.get(selected);
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
			PhiolaQueue qcur = queues.get(selected);
			if (q_filtered != null && filter.length() > filter_len)
				qcur = q_filtered;
			int f = Phiola.QUFILTER_URL | Phiola.QUFILTER_META;
			newqf = qcur.filter(filter, f);
		}

		filter_len = filter.length();

		filter_close();
		q_filtered = newqf;
	}
}
