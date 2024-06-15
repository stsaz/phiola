/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Build;

import androidx.annotation.NonNull;
import androidx.collection.SimpleArrayMap;

import java.util.ArrayList;

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
	String name; // track name shown in GUI
	long pos_msec; // current progress (msec)
	Phiola.Meta pmeta;
	int close_status;
}

class Track {
	private static final String TAG = "phiola.Track";
	private Core core;
	private Phiola phi;
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
		phi = core.phiola;
		phi.playObserverSet(new Phiola.PlayObserver() {
				public void on_create(Phiola.Meta meta) {
					core.tq.post(() -> {
						play_on_create(meta);
					});
				}
				public void on_close(int status) {
					core.tq.post(() -> {
						play_on_close(status);
					});
				}
				public void on_update(long pos_msec) {
					core.tq.post(() -> {
						play_on_update(pos_msec);
					});
				}
			}, 0);
		tplay = new TrackHandle();
		observers = new ArrayList<>();
		tplay.state = STATE_NONE;

		String[] exts = {"mp3", "ogg", "opus", "m4a", "wav", "flac", "mp4", "mkv", "avi"};
		supp_exts = new SimpleArrayMap<>(exts.length);
		for (String e : exts) {
			supp_exts.put(e, true);
		}
	}

	String cur_url() {
		if (tplay.state == STATE_NONE)
			return "";
		return tplay.pmeta.url;
	}

	long curpos_msec() {
		return tplay.pos_msec;
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
		if (t.pmeta.artist.isEmpty()) {
			if (t.pmeta.title.isEmpty()) {
				return Util.path_split2(t.pmeta.url)[1];
			}
			return t.pmeta.title;
		}
		return String.format("%s - %s", t.pmeta.artist, t.pmeta.title);
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

	private void trk_close() {
		tplay.state = STATE_NONE;
		for (int i = observers.size() - 1; i >= 0; i--) {
			PlaybackObserver f = observers.get(i);
			core.dbglog(TAG, "closing observer %s", f);
			f.close(tplay);
		}

		for (int i = observers.size() - 1; i >= 0; i--) {
			PlaybackObserver f = observers.get(i);
			f.closed(tplay);
		}
	}

	/**
	 * Stop playing and notifiy observers
	 */
	void stop() {
		core.dbglog(TAG, "stop");
		trk_close();
	}

	void close(TrackHandle t) {
		core.dbglog(TAG, "close");
		trk_close();
	}

	void pause() {
		core.dbglog(TAG, "pause");
		if (tplay.state == STATE_PLAYING) {
			tplay.state = STATE_PAUSED;
			phi.playCmd(Phiola.PC_PAUSE_TOGGLE, 0);
			update();
		}
	}

	void unpause() {
		core.dbglog(TAG, "unpause: %d", tplay.state);
		if (tplay.state == STATE_PAUSED) {
			tplay.state = STATE_PLAYING;
			phi.playCmd(Phiola.PC_PAUSE_TOGGLE, 0);
			update();
		}
	}

	void seek(long msec) {
		core.dbglog(TAG, "seek: %d", msec);
		if (tplay.state == STATE_PLAYING || tplay.state == STATE_PAUSED) {
			tplay.pos_msec = msec;
			phi.playCmd(Phiola.PC_SEEK, msec);
			update();
		}
	}

	/**
	 * Notify observers on the track's progress
	 */
	private void update() {
		for (PlaybackObserver f : observers) {
			f.process(tplay);
		}
	}

	private void play_on_create(Phiola.Meta m) {
		tplay.state = STATE_PLAYING;
		if (m.artist == null) m.artist = "";
		if (m.title == null) m.title = "";
		if (m.album == null) m.album = "";
		if (m.date == null) m.date = "";
		tplay.pmeta = m;
		tplay.name = Track.header(tplay);

		for (PlaybackObserver f : observers) {
			core.dbglog(TAG, "opening observer %s", f);
			int r = f.open(tplay);
			if (r != 0) {
				core.dbglog(TAG, "f.open(): %d", r);
				trk_close();
				return;
			}
		}
	}

	private void play_on_close(int status) {
		tplay.close_status = status;
		trk_close();
	}

	private void play_on_update(long pos_msec) {
		tplay.pos_msec = pos_msec;
		update();
	}
}
