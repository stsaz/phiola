/** phiola/Android
2024, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.Activity;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.PopupMenu;

import java.util.ArrayList;
import java.util.Arrays;

public class ExplorerMenu {
	private Core core;
	private Activity parent;
	private EditText ctl;
	private String path_left, path;
	private String[] files;
	private boolean up_dir;

	static final int F_MULTI = 1;

	ExplorerMenu(Activity parent) {
		core = Core.getInstance();
		this.parent = parent;
	}

	void show(EditText v, int flags) {
		this.ctl = v;
		String[] rows;

		this.path = this.ctl.getText().toString();
		this.path_left = "";
		if ((flags & F_MULTI) != 0) {
			int sep = this.path.lastIndexOf(";");
			if (sep >= 0) {
				this.path_left = this.path.substring(0, sep + 1);
				this.path = this.path.substring(sep + 1);
			}
		}

		if (this.path.isEmpty()) {
			rows = core.storage_paths;
			files = core.storage_paths;
			this.up_dir = false;

		} else {
			UtilNative.Files f = core.util.dirList(this.path, 0);
			ArrayList<String> a = new ArrayList<>(Arrays.asList(f.display_rows).subList(0, f.n_directories));
			rows = a.toArray(new String[0]);
			files = f.file_names;
			this.up_dir = true;
		}

		PopupMenu m = new PopupMenu(parent, v);
		m.setOnMenuItemClickListener(this::menu_click);
		int i = 0;

		if (this.up_dir)
			m.getMenu().add(0, i++, 0, parent.getString(R.string.explorer_up));

		for (String s : rows) {
			m.getMenu().add(0, i++, 0, s);
		}
		m.show();
	}

	private boolean menu_click(MenuItem item) {
		int i = item.getItemId();
		String s;
		if (i == 0 && this.up_dir) {
			s = Util.path_split2(this.path)[0];
		} else {
			if (this.up_dir)
				i--;
			s = files[i];
		}
		this.ctl.setText(this.path_left + s);
		return true;
	}
}