/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import androidx.annotation.NonNull;

import android.annotation.SuppressLint;
import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.Context;
import android.os.Environment;
import android.util.Log;

class CoreSettings {
	private Core core;
	boolean svc_notification_disable;
	String trash_dir;
	boolean file_del;
	boolean no_tags;
	boolean list_rm_on_next;
	boolean qu_rm_on_err;
	String codepage;
	String pub_data_dir;
	String plist_save_dir;
	String quick_move_dir;

	String rec_path; // directory for recordings
	String rec_enc, rec_fmt;
	int rec_bitrate, rec_buf_len_ms, rec_until_sec, rec_gain_db100;
	boolean rec_danorm, rec_exclusive;
	static final String[] rec_formats = {
		"AAC-LC",
		"AAC-HE",
		"AAC-HEv2",
		"FLAC",
		"Opus"
	};

	String conv_outext;
	int conv_aac_quality;
	int conv_opus_quality;
	int conv_vorbis_quality;
	boolean conv_copy;
	static final String[] conv_extensions = {
		"m4a",
		"opus",
		"ogg",
		"flac",
		"wav",
		"mp3",
	};

	CoreSettings(Core core) {
		this.core = core;
		codepage = "cp1252";
		pub_data_dir = "";
		plist_save_dir = "";
		quick_move_dir = "";
		trash_dir = "Trash";

		rec_path = "";
		rec_enc = "AAC-LC";
		rec_bitrate = 192;
		rec_buf_len_ms = 500;
		rec_until_sec = 3600;

		conv_outext = "m4a";
		conv_aac_quality = 5;
		conv_opus_quality = 192;
		conv_vorbis_quality = 7;
	}

	@SuppressLint("DefaultLocale")
	String writeconf() {
		return String.format("svc_notification_disable %d\n", core.bool_to_int(svc_notification_disable)) +
				String.format("file_delete %d\n", core.bool_to_int(file_del)) +
				String.format("no_tags %d\n", core.bool_to_int(no_tags)) +
				String.format("list_rm_on_next %d\n", core.bool_to_int(list_rm_on_next)) +
				String.format("qu_rm_on_err %d\n", core.bool_to_int(qu_rm_on_err)) +
				String.format("codepage %s\n", codepage) +
				String.format("data_dir %s\n", pub_data_dir) +
				String.format("plist_save_dir %s\n", plist_save_dir) +
				String.format("quick_move_dir %s\n", quick_move_dir) +
				String.format("trash_dir_rel %s\n", trash_dir) +

				String.format("rec_path %s\n", rec_path) +
				String.format("rec_enc %s\n", rec_enc) +
				String.format("rec_bitrate %d\n", rec_bitrate) +
				String.format("rec_buf_len %d\n", rec_buf_len_ms) +
				String.format("rec_until %d\n", rec_until_sec) +
				String.format("rec_danorm %d\n", core.bool_to_int(rec_danorm)) +
				String.format("rec_gain %d\n", rec_gain_db100) +
				String.format("rec_exclusive %d\n", core.bool_to_int(rec_exclusive)) +

				String.format("conv_outext %s\n", conv_outext) +
				String.format("conv_aac_quality %d\n", conv_aac_quality) +
				String.format("conv_opus_quality %d\n", conv_opus_quality) +
				String.format("conv_vorbis_quality %d\n", conv_vorbis_quality) +
				String.format("conv_copy %d\n", core.bool_to_int(conv_copy));
	}

	void set_codepage(String val) {
		if (val.equals("cp1251")
				|| val.equals("cp1252"))
			codepage = val;
		else
			codepage = "cp1252";
	}

	void normalize() {
		if (rec_enc.equals("AAC-LC") || rec_enc.equals("AAC-HE") || rec_enc.equals("AAC-HEv2")) {
			rec_fmt = "m4a";
		} else if (rec_enc.equals("FLAC")) {
			rec_fmt = "flac";
		} else if (rec_enc.equals("Opus")) {
			rec_fmt = "opus";
		} else {
			rec_fmt = "m4a";
			rec_enc = "AAC-LC";
		}

		if (rec_bitrate <= 0)
			rec_bitrate = 192;
		if (rec_buf_len_ms <= 0)
			rec_buf_len_ms = 500;
		if (rec_until_sec < 0)
			rec_until_sec = 3600;
	}

	int readconf(String k, String v) {
		if (k.equals("svc_notification_disable"))
			svc_notification_disable = core.str_to_bool(v);
		else if (k.equals("file_delete"))
			file_del = core.str_to_bool(v);
		else if (k.equals("data_dir"))
			pub_data_dir = v;
		else if (k.equals("plist_save_dir"))
			plist_save_dir = v;
		else if (k.equals("quick_move_dir"))
			quick_move_dir = v;
		else if (k.equals("trash_dir_rel"))
			trash_dir = v;
		else if (k.equals("no_tags"))
			no_tags = core.str_to_bool(v);
		else if (k.equals("list_rm_on_next"))
			list_rm_on_next = core.str_to_bool(v);
		else if (k.equals("qu_rm_on_err"))
			qu_rm_on_err = core.str_to_bool(v);
		else if (k.equals("codepage"))
			set_codepage(v);

		else if (k.equals("rec_path"))
			rec_path = v;
		else if (k.equals("rec_enc"))
			rec_enc = v;
		else if (k.equals("rec_bitrate"))
			rec_bitrate = core.str_to_uint(v, rec_bitrate);
		else if (k.equals("rec_buf_len"))
			rec_buf_len_ms = core.str_to_uint(v, rec_buf_len_ms);
		else if (k.equals("rec_danorm"))
			rec_danorm = core.str_to_bool(v);
		else if (k.equals("rec_exclusive"))
			rec_exclusive = core.str_to_bool(v);
		else if (k.equals("rec_until"))
			rec_until_sec = core.str_to_uint(v, rec_until_sec);
		else if (k.equals("rec_gain"))
			rec_gain_db100 = core.str_to_int(v, rec_gain_db100);

		else if (k.equals("conv_outext"))
			conv_outext = v;
		else if (k.equals("conv_aac_quality"))
			conv_aac_quality = core.str_to_uint(v, conv_aac_quality);
		else if (k.equals("conv_opus_quality"))
			conv_opus_quality = core.str_to_uint(v, conv_opus_quality);
		else if (k.equals("conv_vorbis_quality"))
			conv_vorbis_quality = core.str_to_uint(v, conv_vorbis_quality);
		else if (k.equals("conv_copy"))
			conv_copy = core.str_to_bool(v);
		else
			return 1;
		return 0;
	}
}

class Core extends Util {
	private static Core instance;
	private int refcount;

	private static final String TAG = "phiola.Core";
	private static final String CONF_FN = "phiola-user.conf";
	private static String PUB_DATA_DIR = "phiola";

	private GUI gui;
	private Queue qu;
	private Track track;
	private SysJobs sysjobs;
	private MP mp;
	Phiola phiola;

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

		phiola = new Phiola();
		phiola.storage_paths = storage_paths;
		setts = new CoreSettings(this);
		gui = new GUI(this);
		track = new Track(this);
		qu = new Queue(this);
		mp = new MP();
		mp.init(this);
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

	Track track() {
		return track;
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
		sb.append(this.setts.writeconf());
		sb.append(qu.writeconf());
		sb.append(gui.writeconf());
		dbglog(TAG, sb.toString());
		if (!phiola.confWrite(fn, sb.toString().getBytes()))
			errlog(TAG, "saveconf: %s", fn);
		else
			dbglog(TAG, "saveconf ok: %s", fn);
	}

	/**
	 * Load configuration
	 */
	private void loadconf() {
		String fn = work_dir + "/" + CONF_FN;
		String[] kv = phiola.confRead(fn);
		if (kv == null)
			return;
		for (int i = 0;  i < kv.length;  i += 2) {
			String k = kv[i];
			String v = kv[i+1];
			if (0 == setts.readconf(k, v))
				continue;
			if (0 == qu.readconf(k, v))
				continue;
			gui.readconf(k, v);
		}

		setts.normalize();
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
