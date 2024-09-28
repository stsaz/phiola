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
	private String path;
	private String[] files;
	private boolean up_dir;

	ExplorerMenu(Activity parent) {
		core = Core.getInstance();
		this.parent = parent;
	}

	void show(EditText v) {
		ctl = v;
		String[] rows;

		path = ctl.getText().toString();
		if (path.isEmpty()) {
			rows = core.storage_paths;
			files = core.storage_paths;
			up_dir = false;

		} else {
			UtilNative.Files f = core.util.dirList(path, 0);
			ArrayList<String> a = new ArrayList<>(Arrays.asList(f.display_rows).subList(0, f.n_directories));
			rows = a.toArray(new String[0]);
			files = f.file_names;
			up_dir = true;
		}

		PopupMenu m = new PopupMenu(parent, v);
		m.setOnMenuItemClickListener(this::menu_click);
		int i = 0;

		if (up_dir)
			m.getMenu().add(0, i++, 0, parent.getString(R.string.explorer_up));

		for (String s : rows) {
			m.getMenu().add(0, i++, 0, s);
		}
		m.show();
	}

	private boolean menu_click(MenuItem item) {
		int i = item.getItemId();
		String s;
		if (i == 0 && up_dir) {
			s = Util.path_split2(path)[0];
		} else {
			if (up_dir)
				i--;
			s = files[i];
		}
		ctl.setText(s);
		return true;
	}
}