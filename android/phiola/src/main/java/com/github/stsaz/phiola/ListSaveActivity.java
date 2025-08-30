/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;

import com.github.stsaz.phiola.databinding.ListSaveBinding;

public class ListSaveActivity extends AppCompatActivity {
	private static final String TAG = "phiola.ListSaveActivity";
	private Core core;
	private ExplorerMenu explorer;
	private ListSaveBinding b;

	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = ListSaveBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		core = Core.getInstance();
		explorer = new ExplorerMenu(core, this);

		b.bSave.setOnClickListener((v) -> save());
		b.eDir.setOnClickListener(v -> explorer.show(b.eDir, 0));

		load();

		String name = getIntent().getStringExtra("name");
		if (name == null || name.isEmpty())
			name = "Playlist1";
		b.eName.setText(name);
	}

	protected void onDestroy() {
		core.unref();
		super.onDestroy();
	}

	private void load() {
		b.eDir.setText(core.setts.plist_save_dir);
	}

	private void save() {
		core.setts.plist_save_dir = b.eDir.getText().toString();
		String fn = String.format("%s/%s.m3u8", core.setts.plist_save_dir, b.eName.getText().toString());

		File f = new File(fn);
		if (f.exists()) {
			GUI.dlg_question(this, "File exists"
				, String.format("File '%s' already exists.  Overwrite?", fn)
				, "Overwrite", "Do nothing"
				, (dialog, which) -> { save2(fn); }
				);
			return;
		}
		save2(fn);
	}

	private void save2(String fn) { core.queue().current_save(fn, this::save_complete); }

	private void save_complete(int status) {
		if (status != 0) {
			core.errlog(TAG, "Error saving playlist file");
			return;
		}
		this.finish();
	}
}
