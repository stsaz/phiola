/** phiola/Android: UI utils
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.EditText;
import android.widget.Toast;

class UIUtil {
	static void msg_show(Context ctx, String fmt, Object... args) {
		Toast.makeText(ctx, String.format(fmt, args), Toast.LENGTH_SHORT).show();
	}

	static void dlg_question(Context ctx, String title, String msg, String btn_yes, String btn_no, DialogInterface.OnClickListener on_click) {
		new AlertDialog.Builder(ctx)
			.setTitle(title)
			.setMessage(msg)
			.setPositiveButton(btn_yes, on_click)
			.setNegativeButton(btn_no, null)
			.setIcon(android.R.drawable.ic_dialog_alert)
			.show();
	}

	interface DlgEditOnClick {
		void onClick(String new_text);
	}

	/** Show the dialog with edit text control and yes/no buttons */
	static void dlg_edit(Context ctx, String title, String msg, String text, String btn_yes, String btn_no, DlgEditOnClick on_click) {
		EditText edit_ctl = new EditText(ctx);
		edit_ctl.setText(text);
		new AlertDialog.Builder(ctx)
			.setTitle(title)
			.setMessage(msg)
			.setView(edit_ctl)
			.setPositiveButton(btn_yes, (dialog, which) -> { on_click.onClick(edit_ctl.getText().toString()); })
			.setNegativeButton(btn_no, null)
			.setIcon(android.R.drawable.ic_menu_edit)
			.show();
	}
}
