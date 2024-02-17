/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import androidx.annotation.NonNull;

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
	boolean play_no_tags;
	String codepage;
	String pub_data_dir;
	String plist_save_dir;
	String quick_move_dir;

	String	rec_path; // directory for recordings
	String	rec_enc, rec_fmt;
	int		rec_channels;
	int		rec_rate;
	int		rec_bitrate;
	int		rec_buf_len_ms;
	int		rec_until_sec;
	int		rec_gain_db100;
	boolean	rec_danorm;
	boolean	rec_exclusive;
	static final String[] rec_formats = {
		"AAC-LC",
		"AAC-HE",
		"AAC-HEv2",
		"FLAC",
		"Opus",
		"Opus-VOIP"
	};

	String	conv_outext;
	int		conv_aac_quality;
	int		conv_opus_quality;
	int		conv_vorbis_quality;
	boolean	conv_copy;
	boolean	conv_file_date_preserve;
	boolean	conv_new_add_list;
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
		rec_fmt = "m4a";
		rec_enc = "AAC-LC";
		rec_bitrate = 192;
		rec_buf_len_ms = 500;
		rec_until_sec = 3600;

		conv_outext = "m4a";
		conv_aac_quality = 5;
		conv_opus_quality = 192;
		conv_vorbis_quality = 7;
	}

	String conf_write() {
		return String.format(
			"ui_svc_notfn_disable %d\n"
			+ "play_no_tags %d\n"
			+ "codepage %s\n"
			+ "op_file_delete %d\n"
			+ "op_data_dir %s\n"
			+ "op_plist_save_dir %s\n"
			+ "op_quick_move_dir %s\n"
			+ "op_trash_dir_rel %s\n"
			+ "rec_path %s\n"
			+ "rec_enc %s\n"
			+ "rec_channels %d\n"
			+ "rec_rate %d\n"
			+ "rec_bitrate %d\n"
			+ "rec_buf_len %d\n"
			+ "rec_until %d\n"
			+ "rec_danorm %d\n"
			+ "rec_gain %d\n"
			+ "rec_exclusive %d\n"
			+ "conv_outext %s\n"
			+ "conv_aac_q %d\n"
			+ "conv_opus_q %d\n"
			+ "conv_vorbis_q %d\n"
			+ "conv_copy %d\n"
			+ "conv_file_date_pres %d\n"
			+ "conv_new_add_list %d\n"
			, core.bool_to_int(svc_notification_disable)
			, core.bool_to_int(play_no_tags)
			, codepage
			, core.bool_to_int(file_del)
			, pub_data_dir
			, plist_save_dir
			, quick_move_dir
			, trash_dir
			, rec_path
			, rec_enc
			, rec_channels
			, rec_rate
			, rec_bitrate
			, rec_buf_len_ms
			, rec_until_sec
			, core.bool_to_int(rec_danorm)
			, rec_gain_db100
			, core.bool_to_int(rec_exclusive)
			, conv_outext
			, conv_aac_quality
			, conv_opus_quality
			, conv_vorbis_quality
			, core.bool_to_int(conv_copy)
			, core.bool_to_int(conv_file_date_preserve)
			, core.bool_to_int(conv_new_add_list)
			);
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
		} else if (rec_enc.equals("Opus") || rec_enc.equals("Opus-VOIP")) {
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

	int conf_process1(int k, String v) {
		switch (k) {

		case Conf.UI_SVC_NOTFN_DISABLE:
			svc_notification_disable = core.str_to_bool(v);
			break;

		case Conf.OP_FILE_DELETE:
			file_del = core.str_to_bool(v);
			break;

		case Conf.OP_DATA_DIR:
			pub_data_dir = v;
			break;

		case Conf.OP_PLIST_SAVE_DIR:
			plist_save_dir = v;
			break;

		case Conf.OP_QUICK_MOVE_DIR:
			quick_move_dir = v;
			break;

		case Conf.OP_TRASH_DIR_REL:
			trash_dir = v;
			break;

		case Conf.PLAY_NO_TAGS:
			play_no_tags = core.str_to_bool(v);
			break;

		case Conf.CODEPAGE:
			set_codepage(v);
			break;

		case Conf.REC_PATH:
			rec_path = v;
			break;

		case Conf.REC_ENC:
			rec_enc = v;
			break;

		case Conf.REC_CHANNELS:
			rec_channels = core.str_to_uint(v, 0);
			break;

		case Conf.REC_RATE:
			rec_rate = core.str_to_uint(v, 0);
			break;

		case Conf.REC_BITRATE:
			rec_bitrate = core.str_to_uint(v, rec_bitrate);
			break;

		case Conf.REC_BUF_LEN:
			rec_buf_len_ms = core.str_to_uint(v, rec_buf_len_ms);
			break;

		case Conf.REC_DANORM:
			rec_danorm = core.str_to_bool(v);
			break;

		case Conf.REC_EXCLUSIVE:
			rec_exclusive = core.str_to_bool(v);
			break;

		case Conf.REC_UNTIL:
			rec_until_sec = core.str_to_uint(v, rec_until_sec);
			break;

		case Conf.REC_GAIN:
			rec_gain_db100 = core.str_to_int(v, rec_gain_db100);
			break;

		case Conf.CONV_OUTEXT:
			conv_outext = v;
			break;

		case Conf.CONV_AAC_Q:
			conv_aac_quality = core.str_to_uint(v, conv_aac_quality);
			break;

		case Conf.CONV_OPUS_Q:
			conv_opus_quality = core.str_to_uint(v, conv_opus_quality);
			break;

		case Conf.CONV_VORBIS_Q:
			conv_vorbis_quality = core.str_to_uint(v, conv_vorbis_quality);
			break;

		case Conf.CONV_COPY:
			conv_copy = core.str_to_bool(v);
			break;

		case Conf.CONV_FILE_DATE_PRES:
			conv_file_date_preserve = core.str_to_bool(v);
			break;

		case Conf.CONV_NEW_ADD_LIST:
			conv_new_add_list = core.str_to_bool(v);
			break;

		default:
			return 1;
		}
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
	UtilNative util;
	private Conf conf;

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

		phiola = new Phiola(ctx.getApplicationInfo().nativeLibraryDir);
		conf = new Conf(phiola);
		util = new UtilNative(phiola);
		util.storage_paths = storage_paths;
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
		for (int i = 0;  i < kv.length;  i++) {
			if (kv[i] == null)
				break;

			int k = kv[i].id;
			String v = kv[i].value;
			if (0 == setts.conf_process1(k, v))
				;
			else if (0 == qu.conf_process1(k, v))
				;
			else
				gui.conf_process1(k, v);
		}

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
