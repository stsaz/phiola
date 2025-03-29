/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.EditText;
import android.widget.Toast;

import java.util.ArrayList;

class GUI {
	private static final String TAG = "phiola.GUI";
	private Core core;
	Context cur_activity;
	boolean state_hide;
	boolean filter_hide;
	boolean record_hide;
	boolean ainfo_in_title;
	String cur_path = ""; // current explorer path
	private ArrayList<Integer> list_pos; // list scroll position

	static final int
		THM_DEF = 0,
		THM_DARK = 1;
	int theme;

	static final int
		STATE_DEF = 1,
		STATE_PLAYING = 2,
		STATE_PAUSED = 4,
		MASK_PLAYBACK = 7,
		STATE_AUTO_STOP = 8,
		STATE_RECORDING = 0x10,
		STATE_REC_PAUSED = 0x20,
		STATE_CONVERTING = 0x40;
	int state;

	GUI(Core core) {
		this.core = core;
		list_pos = new ArrayList<>();
	}

	String conf_write() {
		String list_pos_str = "";
		for (Integer it : list_pos) {
			list_pos_str += String.format("%d ", it);
		}

		return String.format(
			"ui_curpath %s\n"
			+ "ui_state_hide %d\n"
			+ "ui_filter_hide %d\n"
			+ "ui_record_hide %d\n"
			+ "ui_info_in_title %d\n"
			+ "ui_list_scroll_pos %s\n"
			+ "ui_theme %d\n"
			, cur_path
			, core.bool_to_int(state_hide)
			, core.bool_to_int(filter_hide)
			, core.bool_to_int(record_hide)
			, core.bool_to_int(ainfo_in_title)
			, list_pos_str
			, theme
			);
	}

	void conf_load(Conf.Entry[] kv) {
		cur_path = kv[Conf.UI_CURPATH].value;
		state_hide = kv[Conf.UI_STATE_HIDE].enabled;
		filter_hide = kv[Conf.UI_FILTER_HIDE].enabled;
		record_hide = kv[Conf.UI_RECORD_HIDE].enabled;
		ainfo_in_title = kv[Conf.UI_INFO_IN_TITLE].enabled;
		String list_pos_str = kv[Conf.UI_LIST_SCROLL_POS].value;
		theme = kv[Conf.UI_THEME].number;

		String[] v = list_pos_str.split(" ");
		for (String s : v) {
			if (!s.isEmpty())
				list_pos.add(Util.str_to_uint(s, 0));
		}
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

	int list_scroll_pos(int i) {
		if (i >= list_pos.size())
			return 0;
		return list_pos.get(i);
	}

	void list_scroll_pos_set(int i, int n) {
		for (int j = list_pos.size();  j <= i;  j++) {
			list_pos.add(0);
		}
		list_pos.set(i, n);
	}

	void list_scroll_pos_swap(int a, int b) {
		int aa = list_scroll_pos(a);
		int bb = list_scroll_pos(b);
		list_scroll_pos_set(a, bb);
		list_scroll_pos_set(b, aa);
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

	static interface DlgEditOnClick {
		public abstract void onClick(String new_text);
	}

	/** Show the dialog with edit text control and yes/no buttons */
	void dlg_edit(Context ctx, String title, String msg, String text, String btn_yes, String btn_no, DlgEditOnClick on_click) {
		EditText edit_ctl = new EditText(ctx);
		edit_ctl.setText(text);
		new AlertDialog.Builder(ctx)
			.setTitle(title)
			.setMessage(msg)
			.setView(edit_ctl)
			.setPositiveButton(btn_yes, (dialog, which) -> { on_click.onClick(edit_ctl.getText().toString()); })
			.setNegativeButton(btn_no, null)
			.setIcon(android.R.drawable.ic_input_get)
			.show();
	}
}
