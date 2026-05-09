/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;
import android.widget.Button;
import android.widget.ToggleButton;

public class About {
	private static final String TAG = "phiola.About";
	private Core core;
	private Context ctx;
	private TextView lAbout;
	private ToggleButton bDebugLog;
	private Button bSaveLogs;

	private Dialog create(Context ctx) {
		View v = LayoutInflater.from(ctx).inflate(R.layout.about, null);
		lAbout = v.findViewById(R.id.lAbout);
		bDebugLog = v.findViewById(R.id.bDebugLog);
		bSaveLogs = v.findViewById(R.id.bSaveLogs);
		lAbout.setText(String.format("v%s\n\n%s",
			core.phiola.version(),
			"https://github.com/stsaz/phiola"));
		bDebugLog.setOnClickListener((view) -> logs_debug());
		bSaveLogs.setOnClickListener((view) -> logs_save_file());

		return new AlertDialog.Builder(ctx)
				.setTitle("About")
				.setView(v)
				.setNegativeButton("Close", null)
				.create();
	}

	private void load() {
		bDebugLog.setChecked(core.setts.debug_logs);
	}

	void show(Context ctx, Core core) {
		this.ctx = ctx;
		this.core = core;
		Dialog dlg = create(ctx);
		load();
		dlg.show();
	}

	private void logs_debug() {
		core.setts.debug_logs = !core.setts.debug_logs;
		core.phiola.setDebug(core.setts.debug_logs);
	}

	private void logs_save_file() {
		String fn = String.format("%s/logs.txt", core.setts.pub_data_dir);
		String[] args = new String[]{
			"logcat", "-f", fn, "-d"
		};
		try {
			Runtime.getRuntime().exec(args);
			GUI.msg_show(ctx, ctx.getString(R.string.about_log_saved), fn);
		} catch (Exception e) {
			core.errlog(TAG, "logs_save_file: %s", e);
		}
	}
}
