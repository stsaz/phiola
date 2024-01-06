/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;

import com.github.stsaz.phiola.databinding.AboutBinding;

public class AboutActivity extends AppCompatActivity {
	private static final String TAG = "phiola.AboutActivity";
	Core core;
	private AboutBinding b;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = AboutBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		core = Core.getInstance();

		b.lAbout.setText(String.format("v%s\n\n%s\n\nTranslations:\n%s",
			core.phiola.version(),
			"https://github.com/stsaz/phiola",
			"https://hosted.weblate.org/projects/phiola/android/"));
		b.bSaveLogs.setOnClickListener((v) -> logs_save_file());
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
	}

	private void logs_save_file() {
		String fn = String.format("%s/logs.txt", core.setts.pub_data_dir);
		String[] args = new String[]{
			"logcat", "-f", fn, "-d"
		};
		try {
			Runtime.getRuntime().exec(args);
			core.gui().msg_show(this, getString(R.string.about_log_saved), fn);
		} catch (Exception e) {
			core.errlog(TAG, "logs_save_file: %s", e);
		}
	}
}
