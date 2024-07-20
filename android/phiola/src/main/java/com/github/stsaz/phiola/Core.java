/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import androidx.annotation.NonNull;

import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.Context;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

class Core extends Util {
	private static Core instance;
	private int refcount;

	private static final String TAG = "phiola.Core";
	private static final String CONF_FN = "phiola-user.conf";
	static String PUB_DATA_DIR = "phiola";

	private GUI gui;
	private Queue qu;
	APlayer aplayer;
	ARecorder arecorder;
	Track track;
	private SysJobs sysjobs;
	Phiola phiola;
	UtilNative util;
	private Conf conf;
	Handler tq;

	String storage_path;
	String[] storage_paths;
	String work_dir;
	Context context;
	CoreSettings setts;

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
			if (0 != c.init(ctx))
				return null;
			instance = c;
			return c;
		}
		return getInstance();
	}

	private int init(@NonNull Context ctx) {
		dbglog(TAG, "init");
		context = ctx;
		work_dir = ctx.getFilesDir().getPath();
		storage_path = Environment.getExternalStorageDirectory().getPath();
		storage_paths = system_storage_dirs(ctx);

		tq = new Handler(Looper.getMainLooper());
		phiola = new Phiola(ctx.getApplicationInfo().nativeLibraryDir, ctx.getAssets());
		conf = new Conf(phiola);
		util = new UtilNative(phiola);
		util.storagePaths(storage_paths);
		setts = new CoreSettings(this);
		gui = new GUI(this);
		if (Build.VERSION.SDK_INT < 26) {
			aplayer = new APlayer(this);
			arecorder = new ARecorder(this);
		}
		track = new Track(this, aplayer);
		qu = new Queue(this);
		sysjobs = new SysJobs();
		sysjobs.init(this);

		loadconf();
		qu.load();
		gui.lists_number(qu.number());
		return 0;
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
		sysjobs.uninit();
		phiola.destroy();
	}

	Queue queue() {
		return qu;
	}

	GUI gui() {
		return gui;
	}

	void saveconf() {
		String fn = work_dir + "/" + CONF_FN;
		StringBuilder sb = new StringBuilder();
		sb.append(this.setts.conf_write());
		sb.append(qu.conf_write());
		sb.append(gui.conf_write());
		dbglog(TAG, "%s", sb.toString());
		if (!conf.confWrite(fn, sb.toString().getBytes()))
			errlog(TAG, "saveconf: %s", fn);
		else
			dbglog(TAG, "saveconf ok: %s", fn);
	}

	private void loadconf() {
		String fn = work_dir + "/" + CONF_FN;
		Conf.Entry[] kv = conf.confRead(fn);
		if (kv != null) {
			setts.conf_load(kv);
			qu.conf_load(kv);
			gui.conf_load(kv);
		}

		setts.normalize();
		qu.conf_normalize();
		phiola.setConfig(setts.codepage, setts.deprecated_mods);
	}

	void clipboard_text_set(Context ctx, String s) {
		ClipboardManager cm = (ClipboardManager)ctx.getSystemService(Context.CLIPBOARD_SERVICE);
		ClipData cd = ClipData.newPlainText("", s);
		cm.setPrimaryClip(cd);
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
		if (r < errors.length)
			return errors[r];
		if ((r & 0x80000000) != 0) // PHI_E_SYS
			return "System";
		return "Other";
	}
}
