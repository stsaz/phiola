/** phiola/Android
2024, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.Activity;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.PopupMenu;

public class VarMenu {
	private Activity parent;
	private EditText ctl;
	private String[] rows;

	VarMenu(Activity parent) {
		this.parent = parent;
	}

	void show(EditText v, String[] rows) {
		ctl = v;
		this.rows = rows;

		PopupMenu m = new PopupMenu(parent, v);
		m.setOnMenuItemClickListener(this::menu_click);
		int i = 0;
		for (String s : rows) {
			m.getMenu().add(0, i++, 0, s);
		}
		m.show();
	}

	private boolean menu_click(MenuItem item) {
		ctl.getText().append(rows[item.getItemId()]);
		return true;
	}
}
