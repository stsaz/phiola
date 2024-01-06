/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;

import com.github.stsaz.phiola.databinding.ListSaveBinding;

public class ListSaveActivity extends AppCompatActivity {
	private static final String TAG = "phiola.ListSaveActivity";
	Core core;
	private ListSaveBinding b;

	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = ListSaveBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		b.bSave.setOnClickListener((v) -> save());

		core = Core.getInstance();
		load();
	}

	protected void onDestroy() {
		core.unref();
		super.onDestroy();
	}

	private void load() {
		b.eDir.setText(core.setts.plist_save_dir);
		b.eName.setText("Playlist1");
	}

	private void save() {
		core.setts.plist_save_dir = b.eDir.getText().toString();
		String fn = String.format("%s/%s.m3u8", core.setts.plist_save_dir, b.eName.getText().toString());
		File f = new File(fn);
		if (f.exists()) {
			core.errlog(TAG, "File already exists.  Please specify a different name.");
			return;
		}
		if (!core.queue().save(fn)) {
			core.errlog(TAG, "Error saving playlist file");
			return;
		}
		this.finish();
	}
}
