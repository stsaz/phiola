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

	private void afocus_prepare() {
		amgr = (AudioManager)core.context.getSystemService(Context.AUDIO_SERVICE);

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

		int r = amgr.requestAudioFocus(focus_obj);
		core.dbglog(TAG, "requestAudioFocus: %d", r);
		afocus = (r == AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
		return afocus;
	}

	void audio_focus_abandon() {
		if (!afocus)
			return;
		afocus = false;
		amgr.abandonAudioFocusRequest(focus_obj);
		core.dbglog(TAG, "abandonAudioFocusRequest");
	}
}
