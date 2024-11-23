/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.content.Context;

import androidx.core.content.ContextCompat;

import java.io.File;
import java.util.ArrayList;

abstract class Util {
	private static final String TAG = "phiola.Util";

	abstract void errlog(String mod, String fmt, Object... args);

	static String int_to_str(int v) {
		return String.format("%d", v);
	}

	static String float_to_str(float v) {
		return String.format("%.02f", v);
	}

	static float str_to_float(String s, float def) {
		try {
			return Float.parseFloat(s);
		} catch (Exception e) {
		}
		return def;
	}

	static int str_to_uint(String s, int def) {
		try {
			int i = Integer.decode(s);
			if (i >= 0)
				return i;
		} catch (Exception e) {
		}
		return def;
	}

	static int str_to_int(String s, int def) {
		try {
			return Integer.decode(s);
		} catch (Exception e) {
		}
		return def;
	}

	static int bool_to_int(boolean b) {
		if (b)
			return 1;
		return 0;
	}

	/**
	 * Get internal and external sdcard paths
	 */
	String[] system_storage_dirs(Context ctx) {
		ArrayList<String> a = new ArrayList<>();
		File[] dirs = ContextCompat.getExternalFilesDirs(ctx, null);
		for (File dir : dirs) {
			if (dir != null) {
				String path = dir.getAbsolutePath(); // "/STORAGE_PATH.../Android/data/..."
				int i = path.indexOf("/Android/data/");
				if (i >= 0)
					a.add(path.substring(0, i));
			}
		}
		return a.toArray(new String[0]);
	}

	/** Convert content provider URL to an absolute file system path.
	e.g. "/document/primary:Music/1.mp3" -> "/storage/emulated/0/Music/1.mp3" */
	static String path_real(String s, String[] storage_paths) {
		int pos = s.indexOf(':');
		if (pos < 0 || pos + 1 == s.length())
			return null;

		String path = s.substring(pos + 1);
		for (String stg : storage_paths) {
			s = stg + "/" + path;
			if (new File(s).exists())
				return s;
		}

		return null;
	}

	boolean file_delete(String path) {
		try {
			File f = new File(path);
			return f.delete();
		} catch (Exception e) {
			errlog(TAG, "file_rename: %s", e);
			return false;
		}
	}

	boolean dir_make(String path) {
		try {
			File f = new File(path);
			return f.mkdir();
		} catch (Exception e) {
			errlog(TAG, "dir_make: %s", e);
			return false;
		}
	}

	/**
	 * Find string in array (case-insensitive)
	 */
	static int array_ifind(String[] array, String search) {
		for (int i = 0; i != array.length; i++) {
			if (search.equalsIgnoreCase(array[i]))
				return i;
		}
		return -1;
	}

	/**
	 * Split full file path into path (without slash) and file name
	 */
	static String[] path_split2(String s) {
		int pos = s.lastIndexOf('/');
		String[] parts = new String[2];
		if (pos != -1) {
			parts[0] = s.substring(0, pos);
			parts[1] = s.substring(pos + 1);
		} else {
			parts[0] = "";
			parts[1] = s;
		}
		return parts;
	}
}
