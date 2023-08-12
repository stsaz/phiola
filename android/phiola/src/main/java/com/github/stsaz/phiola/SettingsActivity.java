/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Build;
import android.os.Bundle;
import android.widget.TextView;

import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;

public class SettingsActivity extends AppCompatActivity {
	private static final String TAG = "phiola.SettingsActivity";
	private Core core;
	private SwitchCompat brandom, brepeat, bfilter_hide, brec_hide, bsvc_notif_disable, bfile_del;
	private SwitchCompat bnotags, blist_rm_on_next, blist_rm_on_err, bdark, ui_info_in_title;
	private TextView tdata_dir, tquick_move_dir, trecdir, ttrash_dir, tautoskip, tcodepage;

	private TextView rec_enc, rec_bitrate, rec_buf_len, rec_until, rec_gain;
	private SwitchCompat rec_exclusive, rec_danorm;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.settings);
		ActionBar actionBar = getSupportActionBar();
		if (actionBar != null)
			actionBar.setDisplayHomeAsUpEnabled(true);

		core = Core.getInstance();
		setup();
		load();
	}

	@Override
	protected void onPause() {
		core.dbglog(TAG, "onPause");
		save();
		super.onPause();
	}

	@Override
	protected void onDestroy() {
		core.dbglog(TAG, "onDestroy");
		core.unref();
		super.onDestroy();
	}

	private void setup() {
		// Interface
		bdark = findViewById(R.id.bdark);
		bdark.setChecked(core.gui().theme == GUI.THM_DARK);
		ui_info_in_title = findViewById(R.id.ui_info_in_title);

		bfilter_hide = findViewById(R.id.bshowfilter);
		bfilter_hide.setChecked(core.gui().filter_hide);

		brec_hide = findViewById(R.id.bshowrec);
		brec_hide.setChecked(core.gui().record_hide);

		bsvc_notif_disable = findViewById(R.id.bsvc_notif_disable);
		bsvc_notif_disable.setChecked(core.setts.svc_notification_disable);

		// Playback
		brandom = findViewById(R.id.brandom);
		brandom.setChecked(core.queue().is_random());

		brepeat = findViewById(R.id.brepeat);
		brepeat.setChecked(core.queue().is_repeat());

		bnotags = findViewById(R.id.bnotags);
		bnotags.setChecked(core.setts.no_tags);

		blist_rm_on_next = findViewById(R.id.blist_rm_on_next);
		blist_rm_on_next.setChecked(core.setts.list_rm_on_next);
		blist_rm_on_err = findViewById(R.id.blist_rm_on_err);

		tcodepage = findViewById(R.id.tcodepage);
		tcodepage.setText(core.setts.codepage);

		tautoskip = findViewById(R.id.tautoskip);
		tautoskip.setText(Integer.toString(core.queue().autoskip_msec / 1000));

		// Operation
		tdata_dir = findViewById(R.id.tdata_dir);
		tdata_dir.setText(core.setts.pub_data_dir);
		tquick_move_dir = findViewById(R.id.tquick_move_dir);

		ttrash_dir = findViewById(R.id.ttrash_dir);
		ttrash_dir.setText(core.setts.trash_dir);

		bfile_del = findViewById(R.id.bfile_del);
		bfile_del.setChecked(core.setts.file_del);

		// Recording
		trecdir = findViewById(R.id.trecdir);
		rec_enc = findViewById(R.id.rec_enc);
		rec_bitrate = findViewById(R.id.rec_bitrate);
		rec_buf_len = findViewById(R.id.rec_buf_len);
		rec_until = findViewById(R.id.rec_until);
		rec_danorm = findViewById(R.id.rec_danorm);
		rec_gain = findViewById(R.id.rec_gain);
		rec_exclusive = findViewById(R.id.rec_exclusive);
	}

	private void load() {
		blist_rm_on_err.setChecked(core.setts.qu_rm_on_err);
		ui_info_in_title.setChecked(core.gui().ainfo_in_title);
		tquick_move_dir.setText(core.setts.quick_move_dir);

		rec_load();
	}

	private void save() {
		core.queue().random(brandom.isChecked());
		core.queue().repeat(brepeat.isChecked());
		core.setts.no_tags = bnotags.isChecked();
		core.setts.list_rm_on_next = blist_rm_on_next.isChecked();
		core.setts.qu_rm_on_err = blist_rm_on_err.isChecked();
		core.setts.set_codepage(tcodepage.getText().toString());
		core.phiola.setCodepage(core.setts.codepage);
		core.queue().autoskip_msec = core.str_to_uint(tautoskip.getText().toString(), 0) * 1000;

		String s = tdata_dir.getText().toString();
		if (s.isEmpty())
			s = core.storage_path + "/phiola";
		core.setts.pub_data_dir = s;

		core.setts.svc_notification_disable = bsvc_notif_disable.isChecked();
		core.setts.trash_dir = ttrash_dir.getText().toString();
		core.setts.file_del = bfile_del.isChecked();

		core.setts.quick_move_dir = tquick_move_dir.getText().toString();

		int i = GUI.THM_DEF;
		if (bdark.isChecked())
			i = GUI.THM_DARK;
		core.gui().theme = i;

		core.gui().filter_hide = bfilter_hide.isChecked();
		core.gui().record_hide = brec_hide.isChecked();
		core.gui().ainfo_in_title = ui_info_in_title.isChecked();

		rec_save();
		core.setts.normalize();
	}

	private void rec_load() {
		trecdir.setText(core.setts.rec_path);
		rec_enc.setText(core.setts.rec_enc);
		rec_bitrate.setText(Integer.toString(core.setts.enc_bitrate));
		rec_buf_len.setText(core.int_to_str(core.setts.rec_buf_len_ms));
		rec_until.setText(core.int_to_str(core.setts.rec_until_sec));
		rec_danorm.setChecked(core.setts.rec_danorm);
		rec_gain.setText(core.float_to_str((float)core.setts.rec_gain_db100 / 100));
		rec_exclusive.setChecked(core.setts.rec_exclusive);

		if (Build.VERSION.SDK_INT < 26) {
			rec_enc.setEnabled(false);
			rec_buf_len.setEnabled(false);
			rec_until.setEnabled(false);
			rec_danorm.setEnabled(false);
			rec_gain.setEnabled(false);
			rec_exclusive.setEnabled(false);
		}
	}

	private void rec_save() {
		core.setts.rec_path = trecdir.getText().toString();
		core.setts.enc_bitrate = core.str_to_uint(rec_bitrate.getText().toString(), -1);
		core.setts.rec_enc = rec_enc.getText().toString();
		core.setts.rec_buf_len_ms = core.str_to_uint(rec_buf_len.getText().toString(), -1);
		core.setts.rec_until_sec = core.str_to_uint(rec_until.getText().toString(), -1);
		core.setts.rec_danorm = rec_danorm.isChecked();
		core.setts.rec_gain_db100 = (int)(core.str_to_float(rec_gain.getText().toString(), 0) * 100);
		core.setts.rec_exclusive = rec_exclusive.isChecked();
	}
}
