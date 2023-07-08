/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;

class SysJobs extends Filter {
	private static final String TAG = "phiola.SysJobs";
	private Core core;
	private Track track;

	private BecomingNoisyReceiver noisy_event;

	private AudioManager amgr;
	private AudioManager.OnAudioFocusChangeListener afocus_change;

	@TargetApi(Build.VERSION_CODES.O)
	private AudioFocusRequest focus_obj;

	private boolean afocus;
	private boolean transient_pause;
	private Handler mloop;
	private Runnable delayed_abandon_focus;

	class BecomingNoisyReceiver extends BroadcastReceiver {
		public void onReceive(Context context, Intent intent) {
			if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction()))
				track.pause();
		}
	}

	void init(Core core) {
		this.core = core;
		track = core.track();
		track.filter_add(this);

		// be notified when headphones are unplugged
		IntentFilter f = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
		noisy_event = new BecomingNoisyReceiver();
		core.context.registerReceiver(noisy_event, f);

		// be notified on audio focus change
		afocus_prepare();
	}

	void uninit() {
		core.context.unregisterReceiver(noisy_event);
	}

	public int open(TrackHandle t) {
		mloop.removeCallbacks(delayed_abandon_focus);
		if (!afocus) {
			int r = afocus_request();
			if (r != AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
				return -1;
			afocus = true;
		}
		return 0;
	}

	public void close(TrackHandle t) {
		if (t.stopped && afocus)
			mloop.postDelayed(delayed_abandon_focus, 1000);
	}

	/**
	 * Prepare audio focus object
	 */
	private void afocus_prepare() {
		amgr = (AudioManager) core.context.getSystemService(Context.AUDIO_SERVICE);
		afocus_change = this::afocus_onchange;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
			AudioFocusRequest.Builder afb = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN);
			afb.setOnAudioFocusChangeListener(afocus_change);

			AudioAttributes.Builder aab = new AudioAttributes.Builder();
			aab.setUsage(AudioAttributes.USAGE_MEDIA);
			aab.setContentType(AudioAttributes.CONTENT_TYPE_MUSIC);

			focus_obj = afb.setAudioAttributes(aab.build()).build();
		}

		mloop = new Handler(core.context.getMainLooper());
		delayed_abandon_focus = this::afocus_abandon;
	}

	/**
	 * Request audio focus
	 */
	private int afocus_request() {
		int r;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
			r = amgr.requestAudioFocus(focus_obj);
		else
			r = amgr.requestAudioFocus(afocus_change, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
		core.dbglog(TAG, "requestAudioFocus: %d", r);
		return r;
	}

	/**
	 * Abandon audio focus
	 */
	private void afocus_abandon() {
		core.dbglog(TAG, "afocus_abandon");
		afocus = false;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
			amgr.abandonAudioFocusRequest(focus_obj);
		else
			amgr.abandonAudioFocus(afocus_change);
	}

	/**
	 * Called by OS when another application starts playing audio
	 */
	private void afocus_onchange(int change) {
		core.dbglog(TAG, "afocus_onchange: %d", change);

		switch (change) {
			case AudioManager.AUDIOFOCUS_LOSS:
				if (afocus)
					afocus_abandon();
				track.stop();
				break;

			case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
			case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
				if (track.state() != Track.STATE_PLAYING)
					break;
				transient_pause = true;
				track.pause();
				break;

			case AudioManager.AUDIOFOCUS_GAIN:
				if (transient_pause) {
					transient_pause = false;
					track.unpause();
				}
				break;
		}
	}
}
