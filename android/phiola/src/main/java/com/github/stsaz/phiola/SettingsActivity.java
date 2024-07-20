/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;

import android.widget.ArrayAdapter;
import android.widget.SeekBar;

import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;

import com.github.stsaz.phiola.databinding.SettingsBinding;

public class SettingsActivity extends AppCompatActivity {
	private static final String TAG = "phiola.SettingsActivity";
	private Core core;
	private SettingsBinding b;
	private ExplorerMenu explorer;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = SettingsBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		ActionBar actionBar = getSupportActionBar();
		if (actionBar != null)
			actionBar.setDisplayHomeAsUpEnabled(true);

		explorer = new ExplorerMenu(this);

		play_init();

		b.eDataDir.setOnClickListener(v -> explorer.show(b.eDataDir));
		b.eQuickMoveDir.setOnClickListener(v -> explorer.show(b.eQuickMoveDir));

		rec_init();

		core = Core.getInstance();
		load();
	}

	private void play_init() {
		b.sbPlayAutoSkip.setMax(auto_skip_progress(200));
		b.sbPlayAutoSkip.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						b.eAutoSkip.setText(AutoSkip.user_str(auto_skip_value(progress)));
					}
				}
			});

		b.sbPlayAutoSkipTail.setMax(auto_skip_progress(200));
		b.sbPlayAutoSkipTail.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						b.eAutoSkipTail.setText(AutoSkip.user_str(auto_skip_value(progress)));
					}
				}
			});

		b.sbPlayAutoStop.setMax(auto_stop_progress(600));
		b.sbPlayAutoStop.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						b.eAutoStop.setText(Util.int_to_str(auto_stop_value(progress)));
					}
				}
			});

		b.swRgNorm.setOnCheckedChangeListener((v, checked) -> {
				if (checked)
					b.swAutoNorm.setChecked(false);
			});
		b.swAutoNorm.setOnCheckedChangeListener((v, checked) -> {
				if (checked)
					b.swRgNorm.setChecked(false);
			});
	}

	private void rec_init() {
		b.eRecDir.setOnClickListener(v -> explorer.show(b.eRecDir));

		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, new String[] {
				"Default",
				"1 (Mono)",
				"2 (Stereo)",
			});
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.spRecChannels.setAdapter(adapter);

		b.sbRecRate.setMax(rec_rate_progress(192000));
		b.sbRecRate.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						String s = "Default";
						if (progress != 0)
							s = core.int_to_str(rec_rate_value(progress));
						b.eRecRate.setText(s);
					}
				}
			});

		b.sbRecBitrate.setMax(rec_bitrate_progress(256));
		b.sbRecBitrate.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eRecBitrate.setText(String.format("%d", rec_bitrate_value(progress)));
				}
			});

		b.sbRecUntil.setMax(rec_until_progress(3600));
		b.sbRecUntil.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eRecUntil.setText(String.format("%d", rec_until_value(progress)));
				}
			});

		adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, CoreSettings.rec_formats);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.spRecEnc.setAdapter(adapter);

		// 0..60
		b.sbRecGain.setMax(60);
		b.sbRecGain.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						String s = "";
						if (progress > 0)
							s = "+";
						b.eRecGain.setText(s + core.int_to_str(progress));
					}
				}
			});

		core = Core.getInstance();

		if (core.aplayer != null) {
			b.swUiInfoInTitle.setEnabled(false);
			b.swListRmOnErr.setEnabled(false);
			b.sbPlayAutoSkip.setEnabled(false);
			b.eAutoSkip.setEnabled(false);
			b.sbPlayAutoSkipTail.setEnabled(false);
			b.eAutoSkipTail.setEnabled(false);
			b.sbPlayAutoStop.setEnabled(false);
			b.eAutoStop.setEnabled(false);
			b.swRgNorm.setEnabled(false);
			b.swAutoNorm.setEnabled(false);
		}

		if (core.arecorder != null) {
			b.eRecName.setEnabled(false);
			b.spRecChannels.setEnabled(false);
			b.sbRecRate.setEnabled(false);
			b.eRecRate.setEnabled(false);
			b.spRecEnc.setEnabled(false);
			b.eRecBufLen.setEnabled(false);
			b.sbRecUntil.setEnabled(false);
			b.eRecUntil.setEnabled(false);
			b.swRecDanorm.setEnabled(false);
			b.sbRecGain.setEnabled(false);
			b.eRecGain.setEnabled(false);
			b.swRecExclusive.setEnabled(false);
		}
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

	private static abstract class SBOnSeekBarChangeListener implements SeekBar.OnSeekBarChangeListener {
		@Override
		public void onStartTrackingTouch(SeekBar seekBar) {}

		@Override
		public void onStopTrackingTouch(SeekBar seekBar) {}
	}

	private void load() {
		// Interface
		b.swDark.setChecked(core.gui().theme == GUI.THM_DARK);
		b.swStateHide.setChecked(core.gui().state_hide);
		b.swShowfilter.setChecked(core.gui().filter_hide);
		b.swShowrec.setChecked(core.gui().record_hide);
		b.swSvcNotifDisable.setChecked(core.setts.svc_notification_disable);
		b.swUiInfoInTitle.setChecked(core.gui().ainfo_in_title);

		// Playlist
		Queue queue = core.queue();
		b.swRandom.setChecked(queue.flags_test(Queue.F_RANDOM));
		b.swRepeat.setChecked(queue.flags_test(Queue.F_REPEAT));
		b.swListAddRmOnNext.setChecked(queue.flags_test(Queue.F_MOVE_ON_NEXT));
		b.swListRmOnNext.setChecked(queue.flags_test(Queue.F_RM_ON_NEXT));
		b.swListRmOnErr.setChecked(queue.flags_test(Queue.F_RM_ON_ERR));
		b.swRgNorm.setChecked(queue.flags_test(Queue.F_RG_NORM));
		b.swAutoNorm.setChecked(queue.flags_test(Queue.F_AUTO_NORM));
		b.sbPlayAutoStop.setProgress(auto_stop_progress(queue.auto_stop_value_min));
		b.eAutoStop.setText(core.int_to_str(queue.auto_stop_value_min));

		// Playback
		b.eCodepage.setText(core.setts.codepage);
		b.sbPlayAutoSkip.setProgress(auto_skip_progress(core.setts.auto_skip_head.val));
		b.eAutoSkip.setText(core.setts.auto_skip_head.str());
		b.sbPlayAutoSkipTail.setProgress(auto_skip_progress(core.setts.auto_skip_tail.val));
		b.eAutoSkipTail.setText(core.setts.auto_skip_tail.str());

		// Operation
		b.eDataDir.setText(core.setts.pub_data_dir);
		b.eTrashDir.setText(core.setts.trash_dir);
		b.swFileDel.setChecked(core.setts.file_del);
		b.eQuickMoveDir.setText(core.setts.quick_move_dir);
		b.swDeprecatedMods.setChecked(core.setts.deprecated_mods);

		rec_load();
	}

	private static int flag(int mask, boolean val) {
		if (val)
			return mask;
		return 0;
	}

	private void save() {
		// Interface
		int i = GUI.THM_DEF;
		if (b.swDark.isChecked())
			i = GUI.THM_DARK;
		core.gui().theme = i;

		core.gui().state_hide = b.swStateHide.isChecked();
		core.gui().filter_hide = b.swShowfilter.isChecked();
		core.gui().record_hide = b.swShowrec.isChecked();
		core.gui().ainfo_in_title = b.swUiInfoInTitle.isChecked();

		// Playlist
		Queue queue = core.queue();

		int f = 0;
		f |= flag(Queue.F_RANDOM, b.swRandom.isChecked());
		f |= flag(Queue.F_REPEAT, b.swRepeat.isChecked());
		f |= flag(Queue.F_MOVE_ON_NEXT, b.swListAddRmOnNext.isChecked());
		f |= flag(Queue.F_RM_ON_NEXT, b.swListRmOnNext.isChecked());
		f |= flag(Queue.F_RM_ON_ERR, b.swListRmOnErr.isChecked());
		f |= flag(Queue.F_RG_NORM, b.swRgNorm.isChecked());
		f |= flag(Queue.F_AUTO_NORM, b.swAutoNorm.isChecked());
		int mask = Queue.F_RANDOM | Queue.F_REPEAT | Queue.F_MOVE_ON_NEXT | Queue.F_RM_ON_NEXT | Queue.F_RM_ON_ERR | Queue.F_RG_NORM | Queue.F_AUTO_NORM;
		queue.flags_set(mask, f);

		queue.auto_stop_value_min = core.str_to_uint(b.eAutoStop.getText().toString(), 0);

		// Playback
		core.setts.set_codepage(b.eCodepage.getText().toString());
		core.setts.auto_skip_head_set(b.eAutoSkip.getText().toString());
		core.setts.auto_skip_tail_set(b.eAutoSkipTail.getText().toString());

		// Operation
		String s = b.eDataDir.getText().toString();
		if (s.isEmpty())
			s = core.storage_path + "/phiola";
		core.setts.pub_data_dir = s;

		core.setts.svc_notification_disable = b.swSvcNotifDisable.isChecked();
		core.setts.trash_dir = b.eTrashDir.getText().toString();
		core.setts.file_del = b.swFileDel.isChecked();
		core.setts.quick_move_dir = b.eQuickMoveDir.getText().toString();
		core.setts.deprecated_mods = b.swDeprecatedMods.isChecked();

		rec_save();
		core.queue().conf_normalize();
		core.setts.normalize();
		core.phiola.setConfig(core.setts.codepage, core.setts.deprecated_mods);
	}

	// 20%..1%; 0; 10sec..200sec by 10
	private int auto_skip_value(int progress) {
		progress -= 20;
		if (progress <= 0)
			return progress;
		return progress * 10;
	}
	private int auto_skip_progress(int n) {
		if (n < 0)
			return 20 - -n;

		if (n <= 200)
			return 20 + n / 10;
		return 20;
	}

	private int auto_stop_value(int progress) { return progress * 6; }
	private int auto_stop_progress(int min) { return min / 6; }

	private int rec_format_index(String s) {
		for (int i = 0; i < CoreSettings.rec_formats.length; i++) {
			if (CoreSettings.rec_formats[i].equals(s))
				return i;
		}
		return -1;
	}

	// Default; 8000..192000 by 1000
	private static int rec_rate_value(int progress) { return 8000 + (progress - 1) * 1000; }
	private static int rec_rate_progress(int rate) { return 1 + (rate - 8000) / 1000; }

	// 8..256 by 8
	private static int rec_bitrate_value(int progress) { return 8 + (progress) * 8; }
	private static int rec_bitrate_progress(int value) { return (value - 8) / 8; }

	// 60..3600 by 60
	private static int rec_until_value(int progress) { return 60 + (progress) * 60; }
	private static int rec_until_progress(int value) { return (value - 60) / 60; }

	private void rec_load() {
		b.swRecLongclick.setChecked(core.setts.rec_longclick);
		b.eRecDir.setText(core.setts.rec_path);
		b.eRecName.setText(core.setts.rec_name_template);
		b.spRecChannels.setSelection(core.setts.rec_channels);
		if (core.setts.rec_rate != 0) {
			b.eRecRate.setText(core.int_to_str(core.setts.rec_rate));
			b.sbRecRate.setProgress(rec_rate_progress(core.setts.rec_rate));
		}
		b.spRecEnc.setSelection(rec_format_index(core.setts.rec_enc));
		b.sbRecBitrate.setProgress(rec_bitrate_progress(core.setts.rec_bitrate));
		b.eRecBitrate.setText(Integer.toString(core.setts.rec_bitrate));
		b.eRecBufLen.setText(core.int_to_str(core.setts.rec_buf_len_ms));
		b.sbRecUntil.setProgress(rec_until_progress(core.setts.rec_until_sec));
		b.eRecUntil.setText(core.int_to_str(core.setts.rec_until_sec));
		b.swRecDanorm.setChecked(core.setts.rec_danorm);
		b.eRecGain.setText(core.float_to_str((float)core.setts.rec_gain_db100 / 100));
		b.sbRecGain.setProgress((int)core.setts.rec_gain_db100 / 100);
		b.swRecExclusive.setChecked(core.setts.rec_exclusive);
		b.swRecListAdd.setChecked(core.setts.rec_list_add);
	}

	private void rec_save() {
		core.setts.rec_longclick = b.swRecLongclick.isChecked();
		core.setts.rec_path = b.eRecDir.getText().toString();
		core.setts.rec_name_template = b.eRecName.getText().toString();
		core.setts.rec_bitrate = core.str_to_uint(b.eRecBitrate.getText().toString(), -1);
		core.setts.rec_channels = b.spRecChannels.getSelectedItemPosition();
		core.setts.rec_rate = core.str_to_uint(b.eRecRate.getText().toString(), 0);
		core.setts.rec_enc = CoreSettings.rec_formats[b.spRecEnc.getSelectedItemPosition()];
		core.setts.rec_buf_len_ms = core.str_to_uint(b.eRecBufLen.getText().toString(), -1);
		core.setts.rec_until_sec = core.str_to_uint(b.eRecUntil.getText().toString(), -1);
		core.setts.rec_danorm = b.swRecDanorm.isChecked();
		core.setts.rec_gain_db100 = (int)(core.str_to_float(b.eRecGain.getText().toString(), 0) * 100);
		core.setts.rec_exclusive = b.swRecExclusive.isChecked();
		core.setts.rec_list_add = b.swRecListAdd.isChecked();
	}
}
