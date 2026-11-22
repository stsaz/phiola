/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.Activity;
import android.content.ComponentName;
import android.support.v4.media.MediaBrowserCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaControllerCompat;
import android.support.v4.media.session.PlaybackStateCompat;

interface TrackCtlIf {
	void close();
	void pause();
	void unpause();
	void next();
	void prev();
	void seek(long pos_msec);
}

class TrackCtlDirect implements TrackCtlIf {
	private Core core;
	TrackCtlDirect(Core core, Activity activity) { this.core = core; }
	public void close() {}
	// public void stop() { core.track.stop(); }
	public void pause() { core.track.pause(); }

	public void unpause() {
		if (core.track.state() == Track.STATE_NONE)
			core.queue().active_play();
		else
			core.track.unpause();
	}

	public void next() { core.queue().order_next(); }
	public void prev() { core.queue().order_prev(); }
	public void seek(long pos_msec) { core.track.seek(pos_msec); }
}

/**
 * Bridge between UI and audio service.
 */
class TrackCtlSys implements TrackCtlIf {
	private static final String TAG = "phiola.TrackCtlSys";
	private Core core;
	private Activity activity;

	private MediaBrowserCompat browser;
	private MediaControllerCompat ctl;

	TrackCtlSys(Core cor, Activity activity) {
		core = cor;
		this.activity = activity;
		browser = new MediaBrowserCompat(core.context, new ComponentName(core.context, MediaSvc.class),
				new MediaBrowserCompat.ConnectionCallback() {
					public void onConnected() {
						core.dbglog(TAG, "MediaBrowserCompat.onConnected");
						on_connected();
					}
				},
				null
		);
		browser.connect();
	}

	public void close() {
		core.dbglog(TAG, "browser.disconnect");
		browser.disconnect();
	}

	/**
	 * Called by MediaBrowser when it's connected to the audio service
	 */
	private void on_connected() {
		try {
			ctl = new MediaControllerCompat(activity, browser.getSessionToken());
		} catch (Exception e) {
			core.errlog(TAG, "%s", e);
			return;
		}
		ctl.registerCallback(new MediaControllerCompat.Callback() {
			public void onSessionReady() {
				core.dbglog(TAG, "MediaControllerCompat.onSessionReady");
			}

			public void onSessionDestroyed() {
				core.dbglog(TAG, "MediaControllerCompat.onSessionDestroyed");
			}

			public void onPlaybackStateChanged(PlaybackStateCompat state) {
				// core.dbglog(TAG, "MediaControllerCompat.onPlaybackStateChanged: %d", state.getState());
			}

			public void onMetadataChanged(MediaMetadataCompat metadata) {
				core.dbglog(TAG, "MediaControllerCompat.onMetadataChanged");
			}
		});
		MediaControllerCompat.setMediaController(activity, ctl);
	}

	/*void play(int pos) {
		ctl.getTransportControls().skipToQueueItem(pos);
	}
	public void stop() {
		core.dbglog(TAG, "stop");
		ctl.getTransportControls().stop();
	}*/

	public void pause() { ctl.getTransportControls().pause(); }
	public void unpause() { ctl.getTransportControls().play(); }
	public void next() { ctl.getTransportControls().skipToNext(); }
	public void prev() { ctl.getTransportControls().skipToPrevious(); }
	public void seek(long pos_msec) { ctl.getTransportControls().seekTo(pos_msec); }
}
