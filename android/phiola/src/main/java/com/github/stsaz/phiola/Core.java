/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import androidx.annotation.NonNull;

import android.app.Activity;
import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;

import androidx.core.app.ActivityCompat;

import java.util.Timer;
import java.util.TimerTask;

class Core extends Util {
	private static Core instance;
	private int refcount;

	private static final String TAG = "phiola.Core";
	private static final String CONF_FN = "phiola-user.conf";
	static String PUB_DATA_DIR = "phiola";

	private GUI gui;
	private Queue qu;
	Track track;
	private SysAudio sysaudio;
	Phiola phiola;
	UtilNative util;
	private Conf conf;
	Handler tq;
	private Runnable delayed_abandon_focus;
	private boolean transient_pause;

	String storage_path;
	String[] storage_paths;
	String work_dir;
	Context context;
	CoreSettings setts;
	PlaySettings play;
	RecSettings rec;
	ConvertSettings convert;

	static Core getInstance() {
		instance.dbglog(TAG, "getInstance");
		instance.refcount++;
		return instance;
	}

	static Core init_once(Context ctx) {
		if (instance == null) {
			if (BuildConfig.DEBUG)
				PUB_DATA_DIR = "phiola-dbg";
			Core c = new Core();
			c.refcount = 1;
			c.init(ctx);
			instance = c;
			return c;
		}
		return getInstance();
	}

	private void init(@NonNull Context ctx) {
		dbglog(TAG, "init");
		context = ctx;
		work_dir = ctx.getFilesDir().getPath();
		storage_path = Environment.getExternalStorageDirectory().getPath();
		storage_paths = system_storage_dirs(ctx);

		tq = new Handler(Looper.getMainLooper());
		phiola = new Phiola(ctx.getAssets());
		conf = new Conf(phiola);
		util = new UtilNative(phiola);
		util.storagePaths(storage_paths);
		setts = new CoreSettings(this);
		play = new PlaySettings(this);
		rec = new RecSettings(this);
		convert = new ConvertSettings(this);
		gui = new GUI(this);
		track = new Track(this);
		qu = new Queue(this);

		sysaudio = new SysAudio(this, new SysAudio.Callback() {
				public void audio_focus(int event) {
					switch (event) {
						case AudioManager.AUDIOFOCUS_LOSS:
							sysaudio.audio_focus_abandon();
							track.stop();
							break;

						case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
						case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
							if (track.state() == Track.STATE_PLAYING) {
								transient_pause = true;
								track.pause();
							}
							break;

						case AudioManager.AUDIOFOCUS_GAIN:
							if (transient_pause) {
								transient_pause = false;
								track.unpause();
							}
							break;
					}
				}

				public void becoming_noisy() {
					track.pause();
				}
			});
		delayed_abandon_focus = sysaudio::audio_focus_abandon;
		track.observer_add(new PlaybackObserver() {
				public int open(TrackHandle t) {
					tq.removeCallbacks(delayed_abandon_focus);
					if (!sysaudio.audio_focus_request())
						return -1;
					return 0;
				}

				public void close(TrackHandle t) {
					if ((t.close_status & Phiola.PCS_STOP) != 0)
						tq.postDelayed(delayed_abandon_focus, 1000);
				}
			});

		loadconf();
		qu.load();
		gui.lists_number(qu.number());
	}

	void unref() {
		dbglog(TAG, "unref(): %d", refcount);
		refcount--;
	}

	void close() {
		dbglog(TAG, "close(): %d", refcount);
		if (--refcount != 0)
			return;
		instance = null;
		qu.close();
		sysaudio.uninit();
		phiola.destroy();
	}

	boolean sys_permisson_request(Activity activity, String[] perms, int code, int ext_stg_mgr_req_code) {
		boolean r = true;

		for (String p : perms) {
			if (ActivityCompat.checkSelfPermission(activity, p) != PackageManager.PERMISSION_GRANTED) {
				ActivityCompat.requestPermissions(activity, perms, code);
				r = false;
				break;
			}
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
			if (ext_stg_mgr_req_code != 0 && !Environment.isExternalStorageManager()) {
				Intent it = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + BuildConfig.APPLICATION_ID));
				ActivityCompat.startActivityForResult(activity, it, ext_stg_mgr_req_code, null);
				r = false;
			}
		}

		return r;
	}

	Queue queue() {
		return qu;
	}

	GUI gui() {
		return gui;
	}

	private String conf_file_name() { return work_dir + "/" + CONF_FN; }

	void saveconf() {
		StringBuilder sb = new StringBuilder();
		sb.append(this.setts.conf_write());
		sb.append(this.play.conf_write());
		sb.append(this.rec.conf_write());
		sb.append(this.convert.conf_write());
		sb.append(qu.conf_write());
		sb.append(gui.conf_write());
		dbglog(TAG, "%s", sb.toString());

		conf.confWrite(conf_file_name(), sb.toString().getBytes());
	}

	private void loadconf() {
		if (conf.confRead(conf_file_name())) {
			setts.conf_load(conf);
			play.conf_load(conf);
			rec.conf_load(conf);
			convert.conf_load(conf);
			qu.conf_load(conf);
			gui.conf_load(conf);
			conf.reset();
		}

		setts.normalize();
		play.normalize();
		rec.normalize();
		convert.normalize();
		qu.conf_normalize();
		phiola.setConfig(setts.codepage, setts.deprecated_mods);
	}

	void clipboard_text_set(Context ctx, String s) {
		ClipboardManager cm = (ClipboardManager)ctx.getSystemService(Context.CLIPBOARD_SERVICE);
		ClipData cd = ClipData.newPlainText("", s);
		cm.setPrimaryClip(cd);
	}

	static class CoreTimer {
		Timer t;
	}

	interface TimerFunc {
		void run();
	}

	CoreTimer timer(int period_msec, TimerFunc cb) {
		CoreTimer t = new CoreTimer();
		t.t = new Timer();
		t.t.schedule(new TimerTask() {
				public void run() {
					tq.post(() -> cb.run());
				}
			}, period_msec, period_msec);
		return t;
	}

	void timer_stop(CoreTimer t) {
		t.t.cancel();
		t.t = null;
	}

	void errlog(String mod, String fmt, Object... args) {
		Log.e(mod, String.format("%s: %s", mod, String.format(fmt, args)));
		if (gui != null)
			gui.on_error(fmt, args);
	}

	void dbglog(String mod, String fmt, Object... args) {
		if (BuildConfig.DEBUG)
			Log.d(mod, String.format("%s: %s", mod, String.format(fmt, args)));
	}

	// enum PHI_E
	private static final String[] errors = {
		"", // PHI_E_OK
		"Input file doesn't exist", // PHI_E_NOSRC
		"Output file already exists", // PHI_E_DSTEXIST
		"Unknown input file format", // PHI_E_UNKIFMT
		"Input audio device problem", // PHI_E_AUDIO_INPUT
		"Cancelled", // PHI_E_CANCELLED
		"Sample conversion", // PHI_E_ACONV
		"Output format is not supported", // PHI_E_OUT_FMT
	};

	String errstr(int r) {
		if ((r & 0x80000000) != 0) // PHI_E_SYS
			return "System";
		if (r < errors.length)
			return errors[r];
		return "Other";
	}
}
