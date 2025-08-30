/** phiola/Android: a wrapper around system MediaPlayer+Timer but with custom observer interface
2024, Simon Zolin */

package com.github.stsaz.phiola;

import android.media.MediaPlayer;

import java.util.Timer;
import java.util.TimerTask;

class APlayer {
	private static final String TAG = "phiola.APlayer";
	private MediaPlayer mp;
	private Timer tmr;
	private Core core;
	private boolean error;
	private long prev_pos_msec;

	Phiola.PlayObserver pobs;
	String url;

	APlayer(Core core) {
		this.core = core;
	}

	int start(String url) {
		if (mp == null) {
			mp = new MediaPlayer();
			mp.setOnPreparedListener((mp) -> on_start());
			mp.setOnCompletionListener((mp) -> on_complete());
			mp.setOnSeekCompleteListener((mp) -> on_seek_complete());
			mp.setOnErrorListener((mp, what, extra) -> {
					on_error();
					return false;
				});
		}

		stop();

		try {
			mp.setDataSource(url);
			this.url = url;
			mp.prepareAsync(); // -> on_start()
		} catch (Exception e) {
			core.errlog(TAG, "MediaPlayer.setDataSource: %s", e);
			return -1;
		}
		return 0;
	}

	private void on_start() {
		core.dbglog(TAG, "prepared");

		Phiola.Meta meta = new Phiola.Meta();
		meta.url = url;
		meta.length_msec = mp.getDuration();
		pobs.on_create(meta);

		tmr = new Timer();
		tmr.schedule(new TimerTask() {
				public void run() {
					core.tq.post(() -> update());
				}
			}, 0, 500);

		prev_pos_msec = -1;
		error = false;
		mp.start(); // ->on_complete(), ->on_error()
	}

	private void update() {
		long pos_msec = mp.getCurrentPosition();
		if (prev_pos_msec < 0
				|| pos_msec / 1000 != prev_pos_msec / 1000) {
			prev_pos_msec = pos_msec;
			pobs.on_update(pos_msec);
		}
	}

	void pause() { mp.pause(); }
	void unpause() { mp.start(); }

	void seek(long pos_msec) {
		mp.seekTo((int)pos_msec); // -> on_seek_complete()
	}

	private void on_seek_complete() {
		core.dbglog(TAG, "on_seek_complete");
	}

	void stop() {
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

	private void on_error() {
		core.dbglog(TAG, "onerror");
		error = true;
		// -> on_complete()
	}

	private void on_complete() {
		core.dbglog(TAG, "completed");
		pobs.on_close(0);
	}
}
