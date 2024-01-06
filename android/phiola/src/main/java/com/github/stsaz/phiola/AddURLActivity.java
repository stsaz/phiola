/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.widget.TextView;

public class AddURLActivity extends AppCompatActivity {
	Core core;
	TextView t_url;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.addurl);
		ActionBar actionBar = getSupportActionBar();
		if (actionBar != null)
			actionBar.setDisplayHomeAsUpEnabled(true);

		core = Core.getInstance();
		t_url = findViewById(R.id.t_url);
		findViewById(R.id.b_url_add).setOnClickListener((v) -> add());
	}

	@Override
	protected void onDestroy() {
		core.unref();
		super.onDestroy();
	}

	private void add() {
		String fn = t_url.getText().toString();
		if (!core.track().supported_url(fn)) {
			core.gui().msg_show(AddURLActivity.this, "Unsupported URL");
			return;
		}
		core.queue().add(fn);
		finish();
	}
}
