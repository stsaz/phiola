/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import java.io.File;

public class ListSave {
	private static final String TAG = "phiola.ListSave";
	private Core core;
	private Context ctx;
	private ExplorerMenu explorer;
	private EditText eDir, eName;

	private Dialog create(Context ctx) {
		View v = LayoutInflater.from(ctx).inflate(R.layout.list_save, null);
		eDir = v.findViewById(R.id.eDir);
		eName = v.findViewById(R.id.eName);
		eDir.setOnClickListener((view) -> explorer.show(eDir, 0));

		return new AlertDialog.Builder(ctx)
				.setTitle("Save Playlist")
				.setView(v)
				.setPositiveButton(ctx.getString(R.string.lssv_bsave), (dialog, which) -> save())
				.setNegativeButton("Cancel", null)
				.create();
	}

	private void load(String name) {
		eDir.setText(core.setts.plist_save_dir);
		if (name == null || name.isEmpty())
			name = "Playlist1";
		eName.setText(name);
	}

	void show(Context ctx, Core core, String name) {
		this.ctx = ctx;
		this.core = core;
		Dialog dlg = create(ctx);
		explorer = new ExplorerMenu(core, ctx);
		load(name);
		dlg.show();
	}

	private void save() {
		core.setts.plist_save_dir = eDir.getText().toString();
		String fn = String.format("%s/%s.m3u8"
			, core.setts.plist_save_dir, eName.getText().toString());

		if (new File(fn).exists()) {
			GUI.dlg_question(ctx, "File exists"
				, String.format("File '%s' already exists.  Overwrite?", fn)
				, "Overwrite", "Do nothing"
				, (dialog, which) -> { save2(fn); }
				);
			return;
		}
		save2(fn);
	}

	private void save2(String fn) {
		core.queue().current_save(fn, this::save_complete);
	}

	private void save_complete(int status) {
		if (status != 0) {
			core.errlog(TAG, "Error saving playlist file");
			return;
		}
	}
}
