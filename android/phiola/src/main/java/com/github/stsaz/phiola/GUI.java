/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.EditText;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.Collections;

class GUI {
	private static final String TAG = "phiola.GUI";
	private final Core core;
	Context cur_activity;
	boolean filter_hide;
	boolean record_hide;
	boolean ainfo_in_title;
	String cur_path = ""; // current explorer path
	private ArrayList<Integer> list_pos; // list scroll position
	private ArrayList<String> list_names;

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
		list_names = new ArrayList<>();
	}

	String conf_write() {
		return String.format(
			"ui_curpath %s\n"
			+ "ui_filter_hide %d\n"
			+ "ui_record_hide %d\n"
			+ "ui_info_in_title %d\n"
			+ "ui_list_scroll_pos %s\n"
			+ "ui_list_names %s\n"
			+ "ui_theme %d\n"
			, cur_path
			, core.bool_to_int(filter_hide)
			, core.bool_to_int(record_hide)
			, core.bool_to_int(ainfo_in_title)
			, list_scroll_pos_string()
			, list_names_string()
			, theme
			);
	}

	void conf_load(Conf.Entry[] kv) {
		cur_path = kv[Conf.UI_CURPATH].value;
		filter_hide = kv[Conf.UI_FILTER_HIDE].enabled;
		record_hide = kv[Conf.UI_RECORD_HIDE].enabled;
		ainfo_in_title = kv[Conf.UI_INFO_IN_TITLE].enabled;
		list_scroll_pos_parse(kv[Conf.UI_LIST_SCROLL_POS].value);
		list_names_parse(kv[Conf.UI_LIST_NAMES].value);
		theme = kv[Conf.UI_THEME].number;
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

	int list_scroll_pos(int i) { return list_pos.get(i); }
	void list_scroll_pos_set(int i, int n) { list_pos.set(i, n); }

	private String list_scroll_pos_string() {
		StringBuilder sb = new StringBuilder();
		for (Integer it : list_pos) {
			sb.append(String.format("%d ", it));
		}
		return sb.toString();
	}

	private void list_scroll_pos_parse(String ss) {
		String[] v = ss.split(" ");
		for (String s : v) {
			if (!s.isEmpty())
				list_pos.add(Util.str_to_uint(s, 0));
		}
	}

	String list_name(int i) { return list_names.get(i); }
	void list_name_set(int i, String name) { list_names.set(i, name); }

	private String list_names_string() {
		StringBuilder sb = new StringBuilder();
		for (String s : list_names) {
			sb.append(String.format("\"%s\" ", s));
		}
		return sb.toString();
	}

	private void list_names_parse(String s) {
		String[] v = s.split("\"");
		for (int i = 1;  i < v.length - 1;  i++) {
			if (v[i].startsWith(" "))
				continue;
			list_names.add(v[i]);
		}
	}

	void list_swap(int a, int b) {
		Collections.swap(list_pos, a, b);
		Collections.swap(list_names, a, b);
	}

	void list_closed(int i) {
		list_pos.remove(i);
		list_names.remove(i);
	}

	void lists_number(int n) {
		if (n < list_pos.size()) {
			list_pos.subList(n, list_pos.size()).clear();
		} else {
			for (int i = list_pos.size();  i < n;  i++) {
				list_pos.add(0);
			}
		}

		if (n < list_names.size()) {
			list_names.subList(n, list_names.size()).clear();
		} else {
			for (int i = list_names.size();  i < n;  i++) {
				list_names.add("");
			}
		}
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
