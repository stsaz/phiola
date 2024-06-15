/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import androidx.annotation.NonNull;

import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.Context;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

class Core extends Util {
	private static Core instance;
	private int refcount;

	private static final String TAG = "phiola.Core";
	private static final String CONF_FN = "phiola-user.conf";
	private static String PUB_DATA_DIR = "phiola";

	private GUI gui;
	private Queue qu;
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
		phiola = new Phiola(ctx.getApplicationInfo().nativeLibraryDir);
		conf = new Conf(phiola);
		util = new UtilNative(phiola);
		util.storagePaths(storage_paths);
		setts = new CoreSettings(this);
		gui = new GUI(this);
		track = new Track(this);
		qu = new Queue(this);
		sysjobs = new SysJobs();
		sysjobs.init(this);

		loadconf();
		if (setts.pub_data_dir.isEmpty())
			setts.pub_data_dir = storage_path + "/" + PUB_DATA_DIR;
		if (setts.plist_save_dir.isEmpty())
			setts.plist_save_dir = setts.pub_data_dir;
		qu.load();
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

	/**
	 * Save configuration
	 */
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

	/**
	 * Load configuration
	 */
	private void loadconf() {
		String fn = work_dir + "/" + CONF_FN;
		Conf.Entry[] kv = conf.confRead(fn);
		if (kv == null)
			return;

		setts.conf_load(kv);
		qu.conf_load(kv);
		gui.conf_load(kv);

		setts.normalize();
		qu.conf_normalize();
		phiola.setCodepage(setts.codepage);
		dbglog(TAG, "loadconf: %s", fn);
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
}
