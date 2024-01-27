/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.content.Context;
import android.widget.Toast;

class GUI {
	private Core core;
	Context cur_activity;
	boolean state_hide;
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

	String conf_write() {
		return String.format(
			"ui_curpath %s\n"
			+ "ui_state_hide %d\n"
			+ "ui_filter_hide %d\n"
			+ "ui_record_hide %d\n"
			+ "list_pos %d\n"
			+ "ui_info_in_title %d\n"
			+ "ui_theme %d\n"
			, cur_path
			, core.bool_to_int(state_hide)
			, core.bool_to_int(filter_hide)
			, core.bool_to_int(record_hide)
			, list_pos
			, core.bool_to_int(ainfo_in_title)
			, theme
			);
	}

	int conf_process1(int k, String v) {
		switch (k) {

		case Conf.UI_CURPATH:
			cur_path = v;
			break;

		case Conf.UI_STATE_HIDE:
			state_hide = core.str_to_bool(v);
			break;

		case Conf.UI_FILTER_HIDE:
			filter_hide = core.str_to_bool(v);
			break;

		case Conf.UI_RECORD_HIDE:
			record_hide = core.str_to_bool(v);
			break;

		case Conf.LIST_POS:
			list_pos = core.str_to_uint(v, 0);
			break;

		case Conf.UI_INFO_IN_TITLE:
			ainfo_in_title = core.str_to_bool(v);
			break;

		case Conf.UI_THEME:
			theme = core.str_to_uint(v, 0);
			break;

		default:
			return 1;
		}
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
