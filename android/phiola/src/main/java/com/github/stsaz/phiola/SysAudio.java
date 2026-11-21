/** phiola/Android: handle system audio events
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.os.Build;

class SysAudio {
	private static final String TAG = "phiola.SysAudio";
	private Core core;
	private AudioManager amgr;
    private boolean afocus;
	private AudioFocusRequest focus_obj;

	class BecomingNoisyReceiver extends BroadcastReceiver {
		public void onReceive(Context context, Intent intent) {
			if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction()))
				cb.becoming_noisy();
		}
	}
	private BecomingNoisyReceiver noisy_event;

	interface Callback {
		void audio_focus(int event);
		void becoming_noisy();
	}
	private Callback cb;

	SysAudio(Core core, Callback cb) {
		this.core = core;
		this.cb = cb;

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

	/** Called by OS when another application starts/stops playing audio */
	private void audio_focus_changed(int event) {
		core.dbglog(TAG, "audio_focus_changed: %d", event);
		cb.audio_focus(event);
	}

	private OnAudioFocusChangeListener focus_listener;

	private void afocus_prepare() {
		amgr = (AudioManager)core.context.getSystemService(Context.AUDIO_SERVICE);

		if (Build.VERSION.SDK_INT < 26) {
			focus_listener = this::audio_focus_changed;
			return;
		}

		AudioFocusRequest.Builder afb = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN);
		afb.setOnAudioFocusChangeListener(this::audio_focus_changed);

		AudioAttributes.Builder aab = new AudioAttributes.Builder();
		aab.setUsage(AudioAttributes.USAGE_MEDIA);
		aab.setContentType(AudioAttributes.CONTENT_TYPE_MUSIC);

		focus_obj = afb.setAudioAttributes(aab.build()).build();
	}

	boolean audio_focus_request() {
		if (afocus)
			return true;

		int r;
		if (Build.VERSION.SDK_INT >= 26)
			r = amgr.requestAudioFocus(focus_obj);
		else
			r = amgr.requestAudioFocus(focus_listener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);

		core.dbglog(TAG, "requestAudioFocus: %d", r);
		afocus = (r == AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
		return afocus;
	}

	void audio_focus_abandon() {
		if (!afocus)
			return;
		afocus = false;
		if (Build.VERSION.SDK_INT >= 26)
			amgr.abandonAudioFocusRequest(focus_obj);
		else
			amgr.abandonAudioFocus(focus_listener);
		core.dbglog(TAG, "abandonAudioFocus");
	}
}
