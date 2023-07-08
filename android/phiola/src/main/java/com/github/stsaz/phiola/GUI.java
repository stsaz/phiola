/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.annotation.SuppressLint;
import android.content.Context;
import android.widget.Toast;

class GUI {
	private Core core;
	Context cur_activity;
	boolean filter_hide;
	boolean record_hide;
	boolean ainfo_in_title;
	String cur_path = ""; // current explorer path
	int list_pos; // list scroll position

	static final int THM_DEF = 0;
	static final int THM_DARK = 1;
	int theme;

	GUI(Core core) {
		this.core = core;
	}

	@SuppressLint("DefaultLocale")
	String writeconf() {
		return String.format("curpath %s\n", cur_path) +
				String.format("filter_hide %d\n", core.bool_to_int(filter_hide)) +
				String.format("record_hide %d\n", core.bool_to_int(record_hide)) +
				String.format("list_pos %d\n", list_pos) +
				String.format("ui_info_in_title %d\n", core.bool_to_int(ainfo_in_title)) +
				String.format("theme %d\n", theme);
	}

	int readconf(String k, String v) {
		if (k.equals("curpath"))
			cur_path = v;
		else if (k.equals("filter_hide"))
			filter_hide = core.str_to_bool(v);
		else if (k.equals("record_hide"))
			record_hide = core.str_to_bool(v);
		else if (k.equals("list_pos"))
			list_pos = core.str_to_uint(v, 0);
		else if (k.equals("ui_info_in_title"))
			ainfo_in_title = core.str_to_bool(v);
		else if (k.equals("theme"))
			theme = core.str_to_uint(v, 0);
		else
			return 1;
		return 0;
	}

	void on_error(String fmt, Object... args) {
		if (cur_activity == null)
			return;
		msg_show(cur_activity, fmt, args);
	}

	/**
	 * Show status message to the user.
	 */
	void msg_show(Context ctx, String fmt, Object... args) {
		Toast.makeText(ctx, String.format(fmt, args), Toast.LENGTH_SHORT).show();
	}
}
