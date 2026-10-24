/** phiola/Android
2023, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import androidx.core.app.NotificationCompat;

public class PlaySvc extends Service {
	private static final String TAG = "phiola.PlaySvc"
		, SWIPE_AWAY = "SWIPE_AWAY";
	static boolean active;
	private Core core;
	private PlaybackObserver play_obs;
	private Runnable play_stop_delayed_task;
	private boolean shown;
	private NotificationCompat.Builder nfy;

	static void start_once(Context ctx) {
		if (!PlaySvc.active)
			ctx.startService(new Intent(ctx, PlaySvc.class));
	}

	private void notif_create() {
		Intent di = new Intent(this, PlaySvc.class);
		di.setAction(SWIPE_AWAY);

		int flags = PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE;
		nfy = new NotificationCompat.Builder(this, core.notif_channel_create("playback", "Playback"))
			.setSmallIcon(R.drawable.ic_phiola)
			.setContentTitle("phiola")
			.setContentText("Playing")
			.setContentIntent(PendingIntent.getActivity(this, 0, new Intent(this, MainActivity.class), flags)) // Launch UI by clicking the notification
			.setDeleteIntent(PendingIntent.getService(this, 0, di, flags)) // Stop playback on notification swipe-away
			;
	}

	@Override
	public void onCreate() {
		PlaySvc.active = true;
		core = Core.getInstance();
		core.dbglog(TAG, "onCreate");

		play_obs = new PlaybackObserver() {
			public void opened(TrackHandle t) { track_new(t); }
			public void close(TrackHandle t) { track_close(t); }
			public void process(TrackHandle t) { track_update(t); }
		};
		core.track.observer_add(play_obs);
		play_stop_delayed_task = this::play_stop;
		notif_create();
		notif_show();
		shown = true;
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		core.dbglog(TAG, "onStartCommand");
		if (intent != null && SWIPE_AWAY.equals(intent.getAction()))
			core.track.stop();
		return START_NOT_STICKY; // Android won't restart the service after it's killed (eg, for more resources)
	}

	@Override
	public IBinder onBind(Intent intent) { return null; }

	@Override
	public void onDestroy() {
		core.dbglog(TAG, "onDestroy");
		core.track.observer_rm(play_obs);
		core.close();
		PlaySvc.active = false;
	}

	private void notif_show() {
		startForeground(/*ID=*/ 1, nfy.build());
	}

	private void notif_hide() {
		stopForeground(STOP_FOREGROUND_REMOVE);
	}

	private void track_new(TrackHandle t) {
		core.tq.removeCallbacks(play_stop_delayed_task);
		notif_show();
	}

	private void track_close(TrackHandle t) {
		core.tq.postDelayed(play_stop_delayed_task, 1000);
	}

	private void track_update(TrackHandle t) {
		if (t.state == Track.STATE_PLAYING && !shown) {
			shown = true;
			notif_show();
		} else if (t.state == Track.STATE_PAUSED && shown) {
			shown = false;
			notif_hide();
		}
	}

	private void play_stop() {
		stopSelf();
	}
}
