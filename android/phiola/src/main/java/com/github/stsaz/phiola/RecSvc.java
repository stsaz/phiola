/** phiola/Android
2023, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;
import androidx.core.app.NotificationCompat;

public class RecSvc extends Service {
	private static final String TAG = "phiola.RecSvc"
		, SWIPE_AWAY = "SWIPE_AWAY";
	private Core core;

	private Notification notification_create() {
		Intent di = new Intent(this, RecSvc.class);
		di.setAction(SWIPE_AWAY);

		int flags = PendingIntent.FLAG_UPDATE_CURRENT
			| PendingIntent.FLAG_IMMUTABLE;
		NotificationCompat.Builder nfy = new NotificationCompat.Builder(this, core.notif_channel_create("worker", "Recording/converting"))
			.setSmallIcon(R.drawable.ic_phiola)
			.setContentTitle("phiola")
			.setContentText("Recording/converting audio")
			.setContentIntent(PendingIntent.getActivity(this, 0, new Intent(this, MainActivity.class), flags)) // Launch UI by clicking the notification
			.setDeleteIntent(PendingIntent.getService(this, 0, di, flags)) // Stop recording on notification swipe-away
			;
		return nfy.build();
	}

	@Override
	public void onCreate() {
		if (BuildConfig.DEBUG)
			Log.d(TAG, "onCreate");

		core = Core.getInstance();
		startForeground(/*ID=*/ 2, notification_create());
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		core.dbglog(TAG, "onStartCommand");
		if (intent != null && SWIPE_AWAY.equals(intent.getAction()))
			core.track.record_stop(false);
		return START_NOT_STICKY;
	}

	@Override
	public IBinder onBind(Intent intent) { return null; }

	@Override
	public void onDestroy() {
		core.dbglog(TAG, "onDestroy");
		core.close();
	}
}
