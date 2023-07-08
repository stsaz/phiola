/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;

public class ListSaveActivity extends AppCompatActivity {
	private static final String TAG = "phiola.ListSaveActivity";
	Core core;
	private EditText tdir, tname;
	private Button bsave;

	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.list_save);

		core = Core.getInstance();

		tdir = findViewById(R.id.lssv_tdir);
		tname = findViewById(R.id.lssv_tname);
		bsave = findViewById(R.id.lssv_bsave);
		bsave.setOnClickListener((v) -> save());

		tdir.setText(core.setts.plist_save_dir);
		tname.setText("Playlist1");
	}

	protected void onDestroy() {
		core.unref();
		super.onDestroy();
	}

	private void save() {
		core.setts.plist_save_dir = tdir.getText().toString();
		String fn = String.format("%s/%s.m3u8", core.setts.plist_save_dir, tname.getText().toString());
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
