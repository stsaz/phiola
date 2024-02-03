/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.v4.media.MediaBrowserCompat;
import android.support.v4.media.MediaDescriptionCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;

import androidx.annotation.NonNull;
import androidx.core.app.NotificationCompat;
import androidx.media.MediaBrowserServiceCompat;
import androidx.media.app.NotificationCompat.MediaStyle;
import androidx.media.session.MediaButtonReceiver;

import java.util.ArrayList;
import java.util.List;

public class Svc extends MediaBrowserServiceCompat {
	private static final String TAG = "phiola.Svc";
	private Core core;
	private Track track;
	private Queue queue;
	private long playtime_msec; // current track's progress

	private MediaSessionCompat sess;
	private int state;
	private PlaybackStateCompat.Builder pstate;
	private NotificationCompat.Builder nfy;
	private Handler mloop;
	private Runnable delayed_stop;

	public void onCreate() {
		super.onCreate();
		init();
	}

	public void onDestroy() {
		core.dbglog(TAG, "onDestroy");
		sess.release();
		core.close();
		super.onDestroy();
	}

	public int onStartCommand(Intent intent, int flags, int startId) {
		core.dbglog(TAG, "onStartCommand");
		MediaButtonReceiver.handleIntent(sess, intent);
		return super.onStartCommand(intent, flags, startId);
	}

	public BrowserRoot onGetRoot(@NonNull String clientPackageName, int clientUid,
								 Bundle rootHints) {
		return new BrowserRoot(getString(R.string.app_name), null);
	}

	public void onLoadChildren(@NonNull final String parentMediaId,
							   @NonNull final Result<List<MediaBrowserCompat.MediaItem>> result) {
		result.sendResult(new ArrayList<>());
	}

	void init() {
		core = Core.init_once(getApplicationContext());
		core.dbglog(TAG, "init");
		queue = core.queue();
		queue.nfy_add(this::sess_setqueue);
		track = core.track();
		track.filter_add(new Filter() {
			public int open(TrackHandle t) {
				return new_track(t);
			}

			public void close(TrackHandle t) {
				close_track(t);
			}

			public int process(TrackHandle t) {
				return update_track(t);
			}
		});

		mloop = new Handler(Looper.getMainLooper());
		delayed_stop = this::stop_delayed;

		sess_init();
		sess_setqueue(0, -1);
		setSessionToken(sess.getSessionToken());
		fg_prep();
	}

	/**
	 * Initialize session
	 */
	void sess_init() {
		ComponentName mbr = new ComponentName(getApplicationContext(), MediaButtonReceiver.class);
		sess = new MediaSessionCompat(getApplicationContext(), TAG, mbr, null);

		pstate = new PlaybackStateCompat.Builder();
		pstate.setActions(PlaybackStateCompat.ACTION_PLAY
				| PlaybackStateCompat.ACTION_STOP
				| PlaybackStateCompat.ACTION_PAUSE
				| PlaybackStateCompat.ACTION_PLAY_PAUSE
				| PlaybackStateCompat.ACTION_SEEK_TO
				| PlaybackStateCompat.ACTION_SKIP_TO_NEXT
				| PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS
				| PlaybackStateCompat.ACTION_SKIP_TO_QUEUE_ITEM
				| PlaybackStateCompat.ACTION_SET_SHUFFLE_MODE
				| PlaybackStateCompat.ACTION_SET_REPEAT_MODE);
		pstate.setActiveQueueItemId(0);
		state = PlaybackStateCompat.STATE_STOPPED;
		pstate.setState(state, 0, 0);
		sess.setPlaybackState(pstate.build());

		int mode = PlaybackStateCompat.SHUFFLE_MODE_NONE;
		if (queue.is_random())
			mode = PlaybackStateCompat.SHUFFLE_MODE_ALL;
		sess.setShuffleMode(mode);

		sess.setCallback(new MediaSessionCompat.Callback() {
			public void onPlay() {
				core.dbglog(TAG, "MediaSessionCompat.onPlay");
				if (track.state() == Track.STATE_NONE) {
					queue.playcur();
					return;
				}
				track.unpause();
				fg();
			}

			public void onPause() {
				core.dbglog(TAG, "MediaSessionCompat.onPause");
				track.pause();
				sess_state(PlaybackStateCompat.STATE_PAUSED, playtime_msec);
				stopForeground(false);
			}

			public void onStop() {
				core.dbglog(TAG, "MediaSessionCompat.onStop");
				track.stop();
			}

			public void onSkipToNext() {
				core.dbglog(TAG, "MediaSessionCompat.onSkipToNext");
				queue.order_next();
			}

			public void onSkipToPrevious() {
				core.dbglog(TAG, "MediaSessionCompat.onSkipToPrevious");
				queue.order_prev();
			}

			public void onSeekTo(long pos) {
				core.dbglog(TAG, "MediaSessionCompat.onSeekTo");
				track.seek((int) pos);
			}

			public boolean onMediaButtonEvent(Intent mediaButtonEvent) {
				core.dbglog(TAG, "MediaSessionCompat.onMediaButtonEvent");
				return super.onMediaButtonEvent(mediaButtonEvent);
			}

			public void onSetShuffleMode(int shuffleMode) {
				core.dbglog(TAG, "MediaSessionCompat.onSetShuffleMode");
				sess.setShuffleMode(shuffleMode);
				queue.random(shuffleMode == PlaybackStateCompat.SHUFFLE_MODE_ALL);
			}

			public void onSetRepeatMode(int repeatMode) {
				core.dbglog(TAG, "MediaSessionCompat.onSetRepeatMode");
				boolean val = (repeatMode == PlaybackStateCompat.REPEAT_MODE_ALL);
				queue.repeat(val);
				sess.setRepeatMode(repeatMode);
			}
		});
	}

	/**
	 * Set session state
	 */
	void sess_state(int st, long playtime_msec) {
		core.dbglog(TAG, "pstate.setState(%d, %d)", st, playtime_msec);
		state = st;
		pstate.setState(state, playtime_msec, 0);
		sess.setPlaybackState(pstate.build());
	}

	/**
	 * Set session queue
	 */
	void sess_setqueue(int how, int pos) {
		String[] l = { "1", "2", "3" };
		ArrayList<MediaSessionCompat.QueueItem> q = new ArrayList<>(l.length);
		int id = 0;
		for (String s : l) {
			MediaDescriptionCompat.Builder b = new MediaDescriptionCompat.Builder();
			b.setMediaId(s);
			MediaSessionCompat.QueueItem i = new MediaSessionCompat.QueueItem(b.build(), id++);
			q.add(i);
		}
		sess.setQueue(q);
	}

	private String notification_channel_create(String id, String name) {
		if (Build.VERSION.SDK_INT < 26)
			return "";

		String r = "";
		NotificationManager mgr = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
		if (mgr != null) {
			NotificationChannel chan = new NotificationChannel(id, name, NotificationManager.IMPORTANCE_LOW);
			mgr.createNotificationChannel(chan);
			r = id;
		}
		return r;
	}

	/**
	 * Prepare notification
	 */
	void fg_prep() {
		nfy = new NotificationCompat.Builder(this, notification_channel_create("phiola.svc.chan", "channame"));
		if (core.setts.svc_notification_disable)
			return;
		nfy.setSmallIcon(R.drawable.ic_phiola);

		// Launch UI by clicking the notification
		Intent intent = new Intent(this, MainActivity.class);
		int flags = PendingIntent.FLAG_UPDATE_CURRENT;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
			flags |= PendingIntent.FLAG_IMMUTABLE;
		PendingIntent contentIntent = PendingIntent.getActivity(this, 0, intent, flags);
		nfy.setContentIntent(contentIntent);

		// Add control buttons
		nfy.addAction(new NotificationCompat.Action(R.drawable.ic_pause, "Pause",
				MediaButtonReceiver.buildMediaButtonPendingIntent(this, PlaybackStateCompat.ACTION_PLAY_PAUSE)));
		nfy.addAction(new NotificationCompat.Action(R.drawable.ic_next, "Next",
				MediaButtonReceiver.buildMediaButtonPendingIntent(this, PlaybackStateCompat.ACTION_SKIP_TO_NEXT)));

		MediaStyle mstyle = new MediaStyle();

		// Show button images
		mstyle.setShowActionsInCompactView(0, 1);
		mstyle.setMediaSession(sess.getSessionToken());

		// Make pause button visible on the lockscreen
		nfy.setVisibility(NotificationCompat.VISIBILITY_PUBLIC);

		// Stop the service when the notification is swiped away (from Android 5)
		nfy.setDeleteIntent(MediaButtonReceiver.buildMediaButtonPendingIntent(this, PlaybackStateCompat.ACTION_STOP));

		// Show Cancel button (before Android 5)
		mstyle.setShowCancelButton(true);
		mstyle.setCancelButtonIntent(MediaButtonReceiver.buildMediaButtonPendingIntent(this, PlaybackStateCompat.ACTION_STOP));

		nfy.setStyle(mstyle);
	}

	/**
	 * Set service as foreground and show notification.
	 */
	void fg() {
		startForeground(1, nfy.build());
		core.dbglog(TAG, "startForeground");
	}

	/**
	 * Called by Track when a new track is initialized
	 */
	int new_track(TrackHandle t) {
		mloop.removeCallbacks(delayed_stop);
		playtime_msec = (int) PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN;
		if (state == PlaybackStateCompat.STATE_STOPPED) {
			sess.setActive(true);
			startService(new Intent(this, Svc.class));
		}

		pstate.setActiveQueueItemId(queue.cur());

		String title = t.title;
		if (t.title.isEmpty())
			title = t.name;

		MediaMetadataCompat.Builder meta = new MediaMetadataCompat.Builder();
		meta.putLong(MediaMetadataCompat.METADATA_KEY_DURATION, t.time_total_msec);
		meta.putString(MediaMetadataCompat.METADATA_KEY_MEDIA_URI, t.url);
		meta.putString(MediaMetadataCompat.METADATA_KEY_TITLE, title);
		if (!t.artist.isEmpty())
			meta.putString(MediaMetadataCompat.METADATA_KEY_ARTIST, t.artist);
		if (!t.album.isEmpty())
			meta.putString(MediaMetadataCompat.METADATA_KEY_ALBUM, t.album);
		// meta.putString(MediaMetadataCompat.METADATA_KEY_TRACK_NUMBER, );
		// meta.putString(MediaMetadataCompat.METADATA_KEY_NUM_TRACKS, );
		sess.setMetadata(meta.build());

		sess_state(PlaybackStateCompat.STATE_BUFFERING, 0);

		if (!core.setts.svc_notification_disable) {
			nfy.setContentTitle(title);
			nfy.setContentText(t.artist);
		}

		fg();
		return 0;
	}

	/**
	 * Called by Track after a track is finished
	 */
	void close_track(TrackHandle t) {
		sess_state(PlaybackStateCompat.STATE_SKIPPING_TO_NEXT, 0);
		mloop.postDelayed(delayed_stop, 1000);
	}

	/**
	 * Called by Track during playback
	 */
	int update_track(TrackHandle t) {
		if (t.state == Track.STATE_PLAYING) {
			playtime_msec = t.pos_msec;
			sess_state(PlaybackStateCompat.STATE_PLAYING, playtime_msec);
		}
		return 0;
	}

	/**
	 * Stop the service after playback is finished
	 */
	void stop_delayed() {
		if (state == PlaybackStateCompat.STATE_PLAYING
				|| state == PlaybackStateCompat.STATE_BUFFERING)
			return;
		sess_state(PlaybackStateCompat.STATE_STOPPED, 0);
		sess.setActive(false);
		stopForeground(true);
		core.dbglog(TAG, "stopForeground, stopSelf");
		stopSelf();
	}
}
