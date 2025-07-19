/** phiola/Android
2025, Simon Zolin */

package com.github.stsaz.phiola;

import java.util.ArrayList;
import java.util.Collections;

/** Music Library is just a quicker way to access the set of .m3u8 playlist files in a user-defined directory. */
class MLib {
	private final MainActivity main;
	private final Core core;
	private String[] display_rows, file_names, display_rows_full, file_names_full;
	private String cur_filter;

	MLib(Core core, MainActivity main) {
		this.core = core;
		this.main = main;
	}

	void fill() {
		if (core.setts.library_dir.isEmpty()) {
			GUI.msg_show(main, "Please set 'Music Library Directories' in Settings");
			return;
		}
		ArrayList<String> rows = new ArrayList<>(), fns = new ArrayList<>();

		String[] dirs = core.setts.library_dir.split(";");
		for (String dir : dirs) {
			UtilNative.Files f = core.util.dirList(dir, 1);

			rows.add(String.format("[%s]", dir));
			fns.add(null);

			Collections.addAll(rows, f.display_rows);
			Collections.addAll(fns, f.file_names);
		}

		this.display_rows = rows.toArray(new String[0]);
		this.file_names = fns.toArray(new String[0]);

		this.display_rows_full = this.display_rows;
		this.file_names_full = this.file_names;
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
		main.explorer_event(this.file_names[pos], Queue.ADD);
	}

	void on_longclick(int pos) {
		if (this.file_names[pos] == null)
			return;
		main.explorer_event(this.file_names[pos], Queue.ADD_RECURSE);
	}
}
