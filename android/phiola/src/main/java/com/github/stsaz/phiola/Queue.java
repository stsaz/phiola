/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import java.io.File;
import java.util.ArrayList;
import java.util.Date;
import java.util.Random;
import java.util.Timer;
import java.util.TimerTask;

class AutoSkip {
	private final Core core;
	int msec, percent;

	AutoSkip(Core core) { this.core = core; }

	/** Set numeric values from string.
	Supports "N%", "N sec" or just "N". */
	void parse(String s) {
		if (!s.isEmpty() && s.charAt(s.length() - 1) == '%') {
			percent = core.str_to_uint(s.substring(0, s.length() - 1), 0);
			if (percent >= 100)
				percent = 0;
			msec = 0;
		} else {
			if (s.indexOf(" sec") > 0)
				s = s.substring(0, s.length() - 4);
			percent = 0;
			msec = core.str_to_uint(s, 0) * 1000;
		}
	}

	String str() {
		if (percent != 0)
			return String.format("%d%%", percent);
		return Integer.toString(msec / 1000);
	}

	long value(long total_msec) {
		if (percent != 0)
			return total_msec * percent / 100;
		return msec;
	}
}

class AutoStop {
	private static final String TAG = "phiola.Queue";
	private final Core core;
	int				value_min;
	private Timer	timer;
	private boolean	active;

	AutoStop(Core core) { this.core = core; }

	boolean armed() { return (timer != null); }

	private void disable() {
		active = false;
		timer.cancel();
		timer = null;
	}

	int toggle() {
		if (timer != null) {
			disable();
			return -1;
		}

		timer = new Timer();
		timer.schedule(new TimerTask() {
				public void run() {
					core.dbglog(TAG, "auto-stop timer: expired");
					active = true;
				}
			}, value_min*60*1000);
		return value_min;
	}

	boolean expired() {
		if (!active)
			return false;

		disable();
		return true;
	}
}

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
	private int trk_idx = -1; // Active track index
	private ArrayList<QueueNotify> nfy;

	private ArrayList<PhiolaQueue> queues;
	private PhiolaQueue q_filtered;
	private int i_active; // Active queue index
	private int i_selected; // Currently selected queue index
	private int curpos = -1; // Last active track index
	private int filter_len;

	private int consecutive_errors;
	private boolean repeat;
	private boolean random;
	private boolean active;
	private Random rnd;
	private boolean random_split;
	boolean add_rm_on_next, rm_on_next;
	boolean rm_on_err;
	AutoSkip auto_skip_beginning, auto_skip_tail;
	AutoStop auto_stop;

	Queue(Core core) {
		this.core = core;
		core.phiola.quSetCallback(this::on_change);
		core.track.filter_add(new Filter() {
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
		auto_skip_beginning = new AutoSkip(core);
		auto_skip_tail = new AutoSkip(core);
		auto_stop = new AutoStop(core);
	}

	private void on_change(long q, int flags, int pos) {
		core.tq.post(() -> {
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
		queues.add(new PhiolaQueue(core.phiola));
		return queues.size() - 1;
	}

	void close_current_list() {
		if (queues.size() == 1) {
			clear();
			return;
		}

		filter_close();
		queues.get(i_selected).destroy();
		queues.remove(i_selected);
		if (i_active == i_selected)
			i_active = 0;
		i_selected--;
		if (i_selected < 0)
			i_selected = 0;

		// As positions of all next lists have just been changed, we must rewrite the files on disk accordingly
		int i = 0;
		for (PhiolaQueue q : queues) {
			if (i++ >= i_selected)
				q.modified = true;
		}
	}

	void close() {
		filter_close();
		for (PhiolaQueue q : queues) {
			q.destroy();
		}
		queues.clear();
		i_selected = -1;
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

		if (i_active >= queues.size())
			i_active = 0;
		if (i_selected >= queues.size())
			i_selected = 0;
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
		i_selected = index_next(i_selected);
		return i_selected;
	}

	void switch_list(int i) { i_selected = i; }

	int current_list_index() { return i_selected; }

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
		if (pos < 0 || pos >= queues.get(i_active).count())
			pos = 0;
		_play(i_active, pos);
	}

	private void _play(int iq, int it) {
		core.dbglog(TAG, "play: %d %d", iq, it);
		if (active)
			core.track.stop();

		if (it < 0 || it >= queues.get(iq).count())
			return;

		i_active = iq;
		trk_idx = it;
		curpos = it;
		core.track.start(it, queues.get(iq).url(it));
	}

	/** Play track at the specified position */
	void play(int index) {
		_play(i_selected, index);
	}

	void visiblelist_play(int i) {
		_play(i_selected, q_visible().index_real(i));
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
		int n = queues.get(i_active).count();
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
		_play(i_active, i);
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

		t.seek_msec = auto_skip_beginning.value(t.time_total_msec);
		t.skip_tail_msec = auto_skip_tail.value(t.time_total_msec);

		if (!core.setts.play_no_tags) {
			queues.get(i_active).modified = true;
			nfy_all(QueueNotify.UPDATE, trk_idx); // redraw item to display artist-title info
		}
	}

	/** Called after a track has been finished. */
	private void on_close(TrackHandle t) {
		active = false;
		boolean play_next = !t.stopped;

		if (trk_idx >= 0 && t.error && rm_on_err) {
			String url = queues.get(i_active).url(trk_idx);
			if (url.equals(t.url))
				remove(trk_idx);
		}

		if (auto_stop.expired()) {
			core.dbglog(TAG, "auto-stop timer: Stopping playback");
			play_next = false;
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
				core.tq.post(this::playcur); // play at current position after the track has been removed
			else
				core.tq.post(this::next);
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

	String get(int i) { return queues.get(i_selected).url(i); }

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
		core.dbglog(TAG, "remove: %d:%d", i_active, pos);
		filter_close();
		queues.get(i_active).remove(pos);
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
			+ "play_auto_skip %s\n"
			+ "play_auto_skip_tail %s\n"
			+ "play_auto_stop %d\n"
			, curpos
			, i_active
			, core.bool_to_int(random)
			, core.bool_to_int(repeat)
			, core.bool_to_int(add_rm_on_next)
			, core.bool_to_int(rm_on_next)
			, core.bool_to_int(rm_on_err)
			, auto_skip_beginning.str()
			, auto_skip_tail.str()
			, auto_stop.value_min
			);
	}

	void conf_load(Conf.Entry[] kv) {
		curpos = core.str_to_uint(kv[Conf.LIST_CURPOS].value, 0);
		i_active = core.str_to_uint(kv[Conf.LIST_ACTIVE].value, 0);
		i_selected = i_active;
		random(kv[Conf.LIST_RANDOM].enabled);
		repeat(kv[Conf.LIST_REPEAT].enabled);
		add_rm_on_next = kv[Conf.LIST_ADD_RM_ON_NEXT].enabled;
		rm_on_next = kv[Conf.LIST_RM_ON_NEXT].enabled;
		rm_on_err = kv[Conf.LIST_RM_ON_ERR].enabled;

		auto_skip_beginning.parse(kv[Conf.PLAY_AUTO_SKIP].value);
		auto_skip_tail.parse(kv[Conf.PLAY_AUTO_SKIP_TAIL].value);
		auto_stop.value_min = core.str_to_uint(kv[Conf.PLAY_AUTO_STOP].value, 0);
	}

	void conf_normalize() {
		if (auto_stop.value_min == 0)
			auto_stop.value_min = 60;
	}

	/** Get currently playing track index */
	int cur() {
		if (i_selected != i_active)
			return -1;
		return trk_idx;
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
}
