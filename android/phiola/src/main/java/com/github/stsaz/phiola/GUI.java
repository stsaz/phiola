/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.Toast;

class GUI {
	private static final String TAG = "phiola.GUI";
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

	static final int
		STATE_DEF = 1,
		STATE_PLAYING = 2,
		STATE_PAUSED = 4,
		MASK_PLAYBACK = 7,
		STATE_AUTO_STOP = 8,
		STATE_RECORDING = 0x10,
		STATE_REC_PAUSED = 0x20;
	int state;

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

	void conf_load(Conf.Entry[] kv) {
		cur_path = kv[Conf.UI_CURPATH].value;
		state_hide = kv[Conf.UI_STATE_HIDE].enabled;
		filter_hide = kv[Conf.UI_FILTER_HIDE].enabled;
		record_hide = kv[Conf.UI_RECORD_HIDE].enabled;
		list_pos = core.str_to_uint(kv[Conf.LIST_POS].value, 0);
		ainfo_in_title = kv[Conf.UI_INFO_IN_TITLE].enabled;
		theme = core.str_to_uint(kv[Conf.UI_THEME].value, 0);
	}

	boolean state_test(int mask) { return (state & mask) != 0; }
	int state_update(int mask, int val) {
		int old = state;
		int _new = (old & ~mask) | val;
		if (_new != old) {
			state = _new;
			core.dbglog(TAG, "state: %x -> %x", old, _new);
		}
		return old;
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

	void dlg_question(Context ctx, String title, String msg, String btn_yes, String btn_no, DialogInterface.OnClickListener on_click) {
		new AlertDialog.Builder(ctx)
			.setTitle(title)
			.setMessage(msg)
			.setPositiveButton(btn_yes, on_click)
			.setNegativeButton(btn_no, null)
			.setIcon(android.R.drawable.ic_dialog_alert)
			.show();
	}
}
