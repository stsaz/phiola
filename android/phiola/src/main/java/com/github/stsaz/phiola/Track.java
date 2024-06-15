/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.media.MediaPlayer;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.collection.SimpleArrayMap;

import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;

abstract class PlaybackObserver {
	/** Called for each new track.
	Return -1: close the track. */
	public int open(TrackHandle t) { return 0; }

	/** Called when track is being closed */
	public void close(TrackHandle t) {}

	/** Called after all observers have been closed */
	public void closed(TrackHandle t) {}

	/** Called periodically while track is playing. */
	public int process(TrackHandle t) { return 0; }
}

class TrackHandle {
	long phi_trk;
	int state;
	boolean stopped; // stopped by user
	boolean error; // processing error
	String url;
	String[] meta;
	String artist, title, album, date, info;
	String name; // track name shown in GUI
	long pos_msec; // current progress (msec)
	long prev_pos_msec; // previous progress position (msec)
	long seek_msec; // default:-1
	long skip_tail_msec;
	long time_total_msec; // track duration (msec)

	void reset() {
		seek_msec = -1;
		prev_pos_msec = -1;
		time_total_msec = 0;
		meta = new String[0];
		artist = "";
		title = "";
		album = "";
		date = "";
		info = "";
	}

	void meta(Phiola.Meta m) {
		meta = m.meta;
		artist = m.artist;
		title = m.title;
		album = m.album;
		date = m.date;
		info = m.info;
		time_total_msec = m.length_msec;
	}
}

class MP {
	private static final String TAG = "phiola.MP";
	private MediaPlayer mp;
	private Timer tmr;
	private Core core;

	MP(Core core) {
		this.core = core;

		core.track.filter_add(new Filter() {
			public int open(TrackHandle t) {
				return on_open(t);
			}

			public void close(TrackHandle t) {
				on_close(t);
			}

			public int process(TrackHandle t) {
				return on_process(t);
			}
		});
	}

	private int on_open(TrackHandle t) {
		t.state = Track.STATE_OPENING;

		if (mp == null)
			mp = new MediaPlayer();
		mp.setOnPreparedListener((mp) -> on_start(t));
		mp.setOnCompletionListener((mp) -> on_complete(t));
		mp.setOnSeekCompleteListener((mp) -> on_seek_complete(t));
		mp.setOnErrorListener((mp, what, extra) -> {
				on_error(t);
				return false;
			});
		try {
			mp.setDataSource(t.url);
			mp.prepareAsync(); // -> on_start()
		} catch (Exception e) {
			core.errlog(TAG, "mp.setDataSource: %s", e);
			return -1;
		}
		return 0;
	}

	private void on_close(TrackHandle t) {
		if (tmr != null) {
			tmr.cancel();
			tmr = null;
		}

		if (mp != null) {
			try {
				mp.stop();
			} catch (Exception ignored) {
			}
			mp.reset();
		}
	}

	private void seek(long pos_msec) {
		if (Build.VERSION.SDK_INT < 26) {
			mp.seekTo((int)pos_msec);
			return;
		}

		mp.seekTo(pos_msec, MediaPlayer.SEEK_PREVIOUS_SYNC);
	}

	private int on_process(TrackHandle t) {
		if (t.seek_msec != -1) {
			if (t.state == Track.STATE_PLAYING)
				t.state = Track.STATE_SEEKING;

			if (t.seek_msec >= t.time_total_msec - t.skip_tail_msec)
				t.skip_tail_msec = 0; // disable auto-skip after manual seek in tail area

			seek(t.seek_msec);
			t.seek_msec = -1;
		}

		if (t.skip_tail_msec > 0
			&& t.pos_msec >= t.time_total_msec - t.skip_tail_msec) {
			on_complete(t);

		} else if (t.state == Track.STATE_PAUSED) {
			mp.pause();
		} else if (t.state == Track.STATE_UNPAUSE) {
			t.state = Track.STATE_PLAYING;
			mp.start();
		}
		return 0;
	}

	/**
	 * Called by MediaPlayer when it's ready to start
	 */
	private void on_start(TrackHandle t) {
		core.dbglog(TAG, "prepared");

		tmr = new Timer();
		tmr.schedule(new TimerTask() {
			public void run() {
				on_timer(t);
			}
		}, 0, 500);

		t.state = Track.STATE_PLAYING;
		t.time_total_msec = mp.getDuration();

		if (t.seek_msec > 0) {
			t.seek_msec = Math.min(t.seek_msec, t.time_total_msec / 2);
			core.dbglog(TAG, "initial seek: %d", t.seek_msec);
			seek(t.seek_msec);
			t.seek_msec = -1;
		}

		mp.start(); // ->on_complete(), ->on_error()
	}

	private void on_seek_complete(TrackHandle t) {
		core.dbglog(TAG, "on_seek_complete");
		if (t.state == Track.STATE_SEEKING)
			t.state = Track.STATE_PLAYING;
	}

	/**
	 * Called by MediaPlayer when it's finished playing
	 */
	private void on_complete(TrackHandle t) {
		core.dbglog(TAG, "completed");
		if (t.state == Track.STATE_NONE)
			return;

		t.stopped = false;
		core.track.close(t);
	}

	private void on_error(TrackHandle t) {
		core.dbglog(TAG, "onerror");
		t.error = true;
		// -> on_complete()
	}

	private void on_timer(TrackHandle t) {
		core.tq.post(() -> update(t));
	}

	private void update(TrackHandle t) {
		if (t.state != Track.STATE_PLAYING)
			return;
		t.pos_msec = mp.getCurrentPosition();
		if (t.prev_pos_msec < 0
				|| t.pos_msec / 1000 != t.prev_pos_msec / 1000) {
			t.prev_pos_msec = t.pos_msec;
			core.track.update(t);
		}
	}
}

/**
 * Chain: Queue -> MP -> SysJobs -> Svc
 */
class Track {
	private static final String TAG = "phiola.Track";
	private Core core;
	private ArrayList<PlaybackObserver> observers;
	private SimpleArrayMap<String, Boolean> supp_exts;

	private TrackHandle tplay;
	private TrackHandle trec;
	private boolean rec_paused;

	static final int
		STATE_NONE = 0,
		STATE_PREPARING = 1, // -> STATE_OPENING
		STATE_OPENING = 2, // -> STATE_PLAYING
		STATE_SEEKING = 3, // -> STATE_PLAYING
		STATE_PLAYING = 4,
		STATE_PAUSED = 5, // -> STATE_UNPAUSE
		STATE_UNPAUSE = 6; // -> STATE_PLAYING

	Track(Core core) {
		this.core = core;
		tplay = new TrackHandle();
		observers = new ArrayList<>();
		tplay.state = STATE_NONE;

		String[] exts = {"mp3", "ogg", "opus", "m4a", "wav", "flac", "mp4", "mkv", "avi"};
		supp_exts = new SimpleArrayMap<>(exts.length);
		for (String e : exts) {
			supp_exts.put(e, true);
		}

		core.dbglog(TAG, "init");
	}

	String cur_url() {
		if (tplay.state == STATE_NONE)
			return "";
		return tplay.url;
	}

	long curpos_msec() {
		return tplay.pos_msec;
	}

	String[] meta() {
		return tplay.meta;
	}

	boolean supported_url(@NonNull String name) {
		if (name.startsWith("/")
			|| name.startsWith("http://") || name.startsWith("https://"))
			return true;
		return false;
	}

	/**
	 * Return TRUE if file name's extension is supported
	 */
	boolean supported(@NonNull String name) {
		if (supported_url(name))
			return true;

		int dot = name.lastIndexOf('.');
		if (dot <= 0)
			return false;
		dot++;
		String ext = name.substring(dot);
		return supp_exts.containsKey(ext);
	}

	void observer_add(PlaybackObserver f) {
		observers.add(f);
	}

	void observer_notify(PlaybackObserver f) {
		if (tplay.state != STATE_NONE) {
			f.open(tplay);
			f.process(tplay);
		}
	}

	void observer_rm(PlaybackObserver f) {
		observers.remove(f);
	}

	int state() {
		return tplay.state;
	}

	private static String header(TrackHandle t) {
		if (t.artist.isEmpty()) {
			if (t.title.isEmpty()) {
				return Util.path_split2(t.url)[1];
			}
			return t.title;
		}
		return String.format("%s - %s", t.artist, t.title);
	}

	/**
	 * Start playing
	 */
	void start(int list_item, String url) {
		core.dbglog(TAG, "play: %s", url);
		if (tplay.state != STATE_NONE)
			return;
		tplay.state = STATE_PREPARING;
		tplay.url = url;
		tplay.reset();

		core.phiola.meta(core.queue().q_active_id(), list_item, url,
			(meta) -> {
				core.tq.post(() -> {
					tplay.meta(meta);
					start_3();
				});
			});
	}

	private void start_3() {
		tplay.name = header(tplay);

		for (PlaybackObserver f : observers) {
			core.dbglog(TAG, "opening observer %s", f);
			int r = f.open(tplay);
			if (r != 0) {
				core.dbglog(TAG, "f.open(): %d", r);
				tplay.error = true;
				trk_close(tplay);
				return;
			}
		}
	}

	private int rec_fmt(String s) {
		if (s.equals("AAC-HE"))
			return Phiola.AF_AAC_HE;
		else if (s.equals("AAC-HEv2"))
			return Phiola.AF_AAC_HE2;
		else if (s.equals("FLAC"))
			return Phiola.AF_FLAC;
		else if (s.equals("Opus"))
			return Phiola.AF_OPUS;
		else if (s.equals("Opus-VOIP"))
			return Phiola.AF_OPUS_VOICE;
		return Phiola.AF_AAC_LC;
	}

	TrackHandle rec_start(String out, Phiola.RecordCallback cb) {
		if (Build.VERSION.SDK_INT < 26) return null;

		Phiola.RecordParams p = new Phiola.RecordParams();
		p.format = rec_fmt(core.setts.rec_enc);
		p.channels = core.setts.rec_channels;
		p.sample_rate = core.setts.rec_rate;

		if (core.setts.rec_exclusive)
			p.flags |= Phiola.RecordParams.RECF_EXCLUSIVE;
		if (true)
			p.flags |= Phiola.RecordParams.RECF_POWER_SAVE;
		if (core.setts.rec_danorm)
			p.flags |= Phiola.RecordParams.RECF_DANORM;

		p.quality = core.setts.rec_bitrate;
		p.buf_len_msec = core.setts.rec_buf_len_ms;
		p.gain_db100 = core.setts.rec_gain_db100;
		p.until_sec = core.setts.rec_until_sec;

		trec = new TrackHandle();
		trec.phi_trk = core.phiola.recStart(out, p, cb);
		if (trec.phi_trk == 0) {
			trec = null;
			return null;
		}
		return trec;
	}

	String record_stop() {
		String e = core.phiola.recCtrl(trec.phi_trk, Phiola.RECL_STOP);
		trec = null;
		return e;
	}

	int record_pause_toggle() {
		if (trec == null) return -1;

		int cmd = Phiola.RECL_PAUSE;
		if (rec_paused)
			cmd = Phiola.RECL_RESUME;
		core.phiola.recCtrl(trec.phi_trk, cmd);

		rec_paused = !rec_paused;
		if (rec_paused)
			return 1;
		return 0;
	}

	private void trk_close(TrackHandle t) {
		t.state = STATE_NONE;
		for (int i = observers.size() - 1; i >= 0; i--) {
			PlaybackObserver f = observers.get(i);
			core.dbglog(TAG, "closing observer %s", f);
			f.close(t);
		}

		for (int i = observers.size() - 1; i >= 0; i--) {
			PlaybackObserver f = observers.get(i);
			f.closed(t);
		}

		t.meta = new String[0];
		t.error = false;
	}

	/**
	 * Stop playing and notifiy observers
	 */
	void stop() {
		core.dbglog(TAG, "stop");
		tplay.stopped = true;
		trk_close(tplay);
	}

	void close(TrackHandle t) {
		core.dbglog(TAG, "close");
		trk_close(t);
	}

	void pause() {
		core.dbglog(TAG, "pause");
		if (tplay.state == STATE_PLAYING) {
			tplay.state = STATE_PAUSED;
			update(tplay);
		}
	}

	void unpause() {
		core.dbglog(TAG, "unpause: %d", tplay.state);
		if (tplay.state == STATE_PAUSED) {
			tplay.state = STATE_UNPAUSE;
			update(tplay);
		}
	}

	void seek(int msec) {
		core.dbglog(TAG, "seek: %d", msec);
		if (tplay.state == STATE_PLAYING || tplay.state == STATE_PAUSED) {
			tplay.seek_msec = msec;
			tplay.pos_msec = msec;
			update(tplay);
		}
	}

	/**
	 * Notify observers on the track's progress
	 */
	void update(TrackHandle t) {
		for (PlaybackObserver f : observers) {
			f.process(t);
		}
	}
}
