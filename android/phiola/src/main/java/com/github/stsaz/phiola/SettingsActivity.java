/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Build;
import android.os.Bundle;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;

import com.github.stsaz.phiola.databinding.SettingsBinding;

public class SettingsActivity extends AppCompatActivity {
	private static final String TAG = "phiola.SettingsActivity";
	private Core core;
	private SettingsBinding b;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = SettingsBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());
		ActionBar actionBar = getSupportActionBar();
		if (actionBar != null)
			actionBar.setDisplayHomeAsUpEnabled(true);

		core = Core.getInstance();
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

	private void load() {
		// Interface
		b.bdark.setChecked(core.gui().theme == GUI.THM_DARK);
		b.bStateHide.setChecked(core.gui().state_hide);
		b.bshowfilter.setChecked(core.gui().filter_hide);
		b.bshowrec.setChecked(core.gui().record_hide);
		b.bsvcNotifDisable.setChecked(core.setts.svc_notification_disable);
		b.blistRmOnErr.setChecked(core.setts.qu_rm_on_err);
		b.uiInfoInTitle.setChecked(core.gui().ainfo_in_title);

		// Playback
		b.brandom.setChecked(core.queue().is_random());
		b.brepeat.setChecked(core.queue().is_repeat());
		b.bnotags.setChecked(core.setts.no_tags);
		b.blistAddRmOnPrev.setChecked(core.setts.list_add_rm_on_prev);
		b.blistRmOnNext.setChecked(core.setts.list_rm_on_next);
		b.tcodepage.setText(core.setts.codepage);
		b.tAutoSkip.setText(core.queue().auto_skip_to_str());
		b.tAutoSkipTail.setText(core.queue().auto_skip_tail_to_str());
		b.tAutoStop.setText(core.int_to_str(core.queue().auto_stop_min));

		// Operation
		b.tdataDir.setText(core.setts.pub_data_dir);
		b.ttrashDir.setText(core.setts.trash_dir);
		b.bfileDel.setChecked(core.setts.file_del);
		b.tquickMoveDir.setText(core.setts.quick_move_dir);

		rec_load();
	}

	private void save() {
		// Interface
		int i = GUI.THM_DEF;
		if (b.bdark.isChecked())
			i = GUI.THM_DARK;
		core.gui().theme = i;

		core.gui().state_hide = b.bStateHide.isChecked();
		core.gui().filter_hide = b.bshowfilter.isChecked();
		core.gui().record_hide = b.bshowrec.isChecked();
		core.gui().ainfo_in_title = b.uiInfoInTitle.isChecked();

		// Playback
		core.queue().random(b.brandom.isChecked());
		core.queue().repeat(b.brepeat.isChecked());
		core.setts.no_tags = b.bnotags.isChecked();
		core.setts.list_add_rm_on_prev = b.blistAddRmOnPrev.isChecked();
		core.setts.list_rm_on_next = b.blistRmOnNext.isChecked();
		core.setts.qu_rm_on_err = b.blistRmOnErr.isChecked();
		core.setts.set_codepage(b.tcodepage.getText().toString());
		core.phiola.setCodepage(core.setts.codepage);
		core.queue().auto_skip(b.tAutoSkip.getText().toString());
		core.queue().auto_skip_tail(b.tAutoSkipTail.getText().toString());
		core.queue().auto_stop_min = core.str_to_uint(b.tAutoStop.getText().toString(), 0);

		// Operation
		String s = b.tdataDir.getText().toString();
		if (s.isEmpty())
			s = core.storage_path + "/phiola";
		core.setts.pub_data_dir = s;

		core.setts.svc_notification_disable = b.bsvcNotifDisable.isChecked();
		core.setts.trash_dir = b.ttrashDir.getText().toString();
		core.setts.file_del = b.bfileDel.isChecked();
		core.setts.quick_move_dir = b.tquickMoveDir.getText().toString();

		rec_save();
		core.queue().conf_normalize();
		core.setts.normalize();
	}

	private int rec_format_index(String s) {
		for (int i = 0;  i < core.setts.rec_formats.length;  i++) {
			if (core.setts.rec_formats[i].equals(s))
				return i;
		}
		return -1;
	}

	private void rec_load() {
		b.trecdir.setText(core.setts.rec_path);

		ArrayAdapter<String> adapter = new ArrayAdapter<>(
			this,
			android.R.layout.simple_spinner_item,
			core.setts.rec_formats
		);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.recEnc.setAdapter(adapter);
		b.recEnc.setSelection(rec_format_index(core.setts.rec_enc));

		b.recBitrate.setText(Integer.toString(core.setts.rec_bitrate));
		b.recBufLen.setText(core.int_to_str(core.setts.rec_buf_len_ms));
		b.recUntil.setText(core.int_to_str(core.setts.rec_until_sec));
		b.recDanorm.setChecked(core.setts.rec_danorm);
		b.recGain.setText(core.float_to_str((float)core.setts.rec_gain_db100 / 100));
		b.recExclusive.setChecked(core.setts.rec_exclusive);

		if (Build.VERSION.SDK_INT < 26) {
			b.recEnc.setEnabled(false);
			b.recBufLen.setEnabled(false);
			b.recUntil.setEnabled(false);
			b.recDanorm.setEnabled(false);
			b.recGain.setEnabled(false);
			b.recExclusive.setEnabled(false);
		}
	}

	private void rec_save() {
		core.setts.rec_path = b.trecdir.getText().toString();
		core.setts.rec_bitrate = core.str_to_uint(b.recBitrate.getText().toString(), -1);
		core.setts.rec_enc = core.setts.rec_formats[b.recEnc.getSelectedItemPosition()];
		core.setts.rec_buf_len_ms = core.str_to_uint(b.recBufLen.getText().toString(), -1);
		core.setts.rec_until_sec = core.str_to_uint(b.recUntil.getText().toString(), -1);
		core.setts.rec_danorm = b.recDanorm.isChecked();
		core.setts.rec_gain_db100 = (int)(core.str_to_float(b.recGain.getText().toString(), 0) * 100);
		core.setts.rec_exclusive = b.recExclusive.isChecked();
	}
}
