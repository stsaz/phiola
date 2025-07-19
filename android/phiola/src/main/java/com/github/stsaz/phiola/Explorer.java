/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;

import androidx.core.app.ActivityCompat;

class Explorer {
	private static final String TAG = "phiola.Explorer";
	private final Core core;
	private final GUI gui;
	private final MainActivity main;

	private boolean up_dir; // Show "UP" directory link
	private String parent, display_path;
	private String[] display_rows, file_names;
	private int n_dirs;

	Explorer(Core core, MainActivity main) {
		this.core = core;
		this.main = main;
		gui = core.gui();
	}

	int count() {
		if (display_rows == null)
			return 0;
		int n = 1;
		if (up_dir)
			n++;
		return display_rows.length + n;
	}

	String display_line(int pos) {
		if (pos == 0)
			return display_path;
		pos--;

		if (up_dir) {
			if (pos == 0)
				return main.getString(R.string.explorer_up);
			pos--;
		}

		return display_rows[pos];
	}

	void event(int ev, int pos) {
		core.dbglog(TAG, "click on %d", pos);
		if (pos == 0)
			return; // click on the current directory path
		pos--;

		if (up_dir) {
			if (pos == 0) {
				if (ev == 1)
					return; // long click on "<UP>"

				if (parent == null)
					list_show_root();
				else
					list_show(parent);
				main.explorer_event(null, 0);
				return;
			}
			pos--;
		}

		if (ev == 1) {
			main.explorer_event(file_names[pos], Queue.ADD_RECURSE);
			return;
		}

		if (pos < n_dirs) {
			list_show(file_names[pos]);
			main.explorer_event(null, 0);
			return;
		}

		main.explorer_event(file_names[pos], Queue.ADD);
	}

	void fill() {
		if (gui.cur_path.isEmpty())
			list_show_root();
		else
			list_show(gui.cur_path);
	}

	/** Read directory contents.
	Request user permission to access file system. */
	private void list_show(String path) {
		core.dbglog(TAG, "list_show: %s", path);

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
			if (!Environment.isExternalStorageManager()) {
				Intent it = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION
						, Uri.parse("package:" + BuildConfig.APPLICATION_ID));
				ActivityCompat.startActivityForResult(main, it, MainActivity.REQUEST_STORAGE_ACCESS, null);
			}
		}

		UtilNative.Files f = core.util.dirList(path, 0);
		file_names = f.file_names;
		display_rows = f.display_rows;
		n_dirs = f.n_directories;
		core.dbglog(TAG, "%d entries, %d directories", file_names.length, n_dirs);

		display_path = String.format("[%s]", path);
		gui.cur_path = path;

		/* Prevent from going upper than sdcard because
		 it may be impossible to come back (due to file permissions) */
		parent = null;
		if (Util.array_ifind(core.storage_paths, path) < 0)
			parent = Util.path_split2(path)[0];

		up_dir = true;
	}

	/** Show the list of all available storage directories. */
	private void list_show_root() {
		file_names = core.storage_paths;
		n_dirs = core.storage_paths.length;
		display_rows = core.storage_paths;

		display_path = main.getString(R.string.explorer_stg_dirs);
		gui.cur_path = "";
		up_dir = false;
	}

	int file_idx(String fn) {
		for (int i = 0; i != file_names.length; i++) {
			if (file_names[i].equalsIgnoreCase(fn)) {
				return i;
			}
		}
		return -1;
	}
}
