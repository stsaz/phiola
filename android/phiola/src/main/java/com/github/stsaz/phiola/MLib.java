/** phiola/Android
2025, Simon Zolin */

package com.github.stsaz.phiola;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;

/** Music Library is just a quicker way to access the set of .m3u8 playlist files in a user-defined directory. */
class MLib {
	private static final String TAG = "phiola.MLib";
	private final MainActivity main;
	private final Core core;
	private String[] display_rows, file_names, display_rows_full, file_names_full;
	private String lib_dir_cached, cur_filter;
	private long mtime_cached;

	MLib(Core core, MainActivity main) {
		this.core = core;
		this.main = main;
	}

	private boolean cached(String[] dirs) {
		long mtime_latest = 0;
		for (String dir : dirs) {
			long mt = new File(dir).lastModified();
			if (mtime_latest < mt)
				mtime_latest = mt;
		}

		if (this.lib_dir_cached != null
			&& this.lib_dir_cached.equals(core.setts.library_dir)
			&& mtime_latest <= this.mtime_cached) {
			core.dbglog(TAG, "cached");
			return true;
		}

		this.mtime_cached = mtime_latest;
		this.lib_dir_cached = core.setts.library_dir;
		return false;
	}

	void fill() {
		if (core.setts.library_dir.isEmpty()) {
			GUI.msg_show(main, "Please set 'Music Library Directories' in Settings");
			return;
		}

		String[] dirs = core.setts.library_dir.split(";");
		if (!cached(dirs)) {
			ArrayList<String> rows = new ArrayList<>(), fns = new ArrayList<>();
			for (String dir : dirs) {
				UtilNative.Files f = core.util.dirList(dir, 1);

				rows.add(String.format("[%s]", dir));
				fns.add(null);

				Collections.addAll(rows, f.display_rows);
				Collections.addAll(fns, f.file_names);
			}

			this.display_rows_full = rows.toArray(new String[0]);
			this.file_names_full = fns.toArray(new String[0]);
		}

		this.display_rows = this.display_rows_full;
		this.file_names = this.file_names_full;
		this.cur_filter = null;
	}

	void filter(String filter) {
		boolean advance = (this.cur_filter != null
			&& filter.length() > this.cur_filter.length());
		this.cur_filter = filter;
		if (filter.isEmpty() || !advance) {
			this.display_rows = this.display_rows_full;
			this.file_names = this.file_names_full;
			if (filter.isEmpty())
				return;
		}

		ArrayList<String> rows = new ArrayList<>(), fns = new ArrayList<>();
		filter = filter.toLowerCase();
		int i = 0;
		for (String s : display_rows) {
			if (s.toLowerCase().contains(filter)) {
				rows.add(s);
				fns.add(this.file_names[i]);
			}
			i++;
		}

		this.display_rows = rows.toArray(new String[0]);
		this.file_names = fns.toArray(new String[0]);
	}

	int count() {
		if (this.display_rows == null)
			return 0;
		return this.display_rows.length;
	}

	String display_line(int pos) {
		return this.display_rows[pos];
	}

	void on_click(int pos) {
		if (this.file_names[pos] == null)
			return;
		main.library_event(this.file_names[pos], 0);
	}

	void on_longclick(int pos) {
		if (this.file_names[pos] == null)
			return;
		main.library_event(this.file_names[pos], 1);
	}
}
