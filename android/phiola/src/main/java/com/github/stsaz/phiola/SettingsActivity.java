/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;

import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.SeekBar;

import androidx.appcompat.app.AppCompatActivity;

import com.github.stsaz.phiola.databinding.SettingsBinding;

import java.lang.Math;
import java.util.ArrayList;
import java.util.Arrays;

public class SettingsActivity extends AppCompatActivity {
	private static final String TAG = "phiola.SettingsActivity";
	private Core core;
	private SettingsBinding b;
	private ExplorerMenu explorer;
	private int color;
	private String equ;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = SettingsBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		core = Core.getInstance();
		core.gui().on_activity_show(this);
		explorer = new ExplorerMenu(core, this);

		ui_init();
		play_init();

		b.eDataDir.setOnClickListener(v -> explorer.show(b.eDataDir, 0));
		b.eLibraryDir.setOnClickListener(v -> explorer.show(b.eLibraryDir, ExplorerMenu.F_MULTI));

		rec_init();

		load();
	}

	private void ui_init() {
		b.bColor.setOnClickListener(v -> new DlgColor().show(this, color, (color) -> {
			this.color = color;
		}));
	}

	private ArrayAdapter<String> spinner_adapter(String[] options) {
		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, options);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		return adapter;
	}

	private void play_init() {
		SeekBar.OnSeekBarChangeListener sbcl = new SeekBar.OnSeekBarChangeListener() {
			@Override
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				if (fromUser) {
					switch (seekBar.getId()) {
					case R.id.sbPlayAutoSkip:
						b.eAutoSkip.setText(AutoSkip.user_str(auto_skip_value(progress)));
						break;

					case R.id.sbPlayAutoSkipTail:
						b.eAutoSkipTail.setText(AutoSkip.user_str(auto_skip_value(progress)));
						break;

					case R.id.sbPlayAutoStop:
						b.eAutoStop.setText(Util.int_to_str(auto_stop_value(progress)));
						break;
					}
				}
			}

			@Override
			public void onStartTrackingTouch(SeekBar seekBar) {}

			@Override
			public void onStopTrackingTouch(SeekBar seekBar) {}
		};
		b.sbPlayAutoSkip.setOnSeekBarChangeListener(sbcl);
		b.sbPlayAutoSkipTail.setOnSeekBarChangeListener(sbcl);
		b.sbPlayAutoStop.setOnSeekBarChangeListener(sbcl);

		b.sbPlayAutoSkip.setMax(auto_skip_progress(200));
		b.sbPlayAutoSkipTail.setMax(auto_skip_progress(200));
		b.sbPlayAutoStop.setMax(auto_stop_progress(6*60));

		b.swRgNorm.setOnCheckedChangeListener((v, checked) -> {
				if (checked)
					b.swAutoNorm.setChecked(false);
			});
		b.swAutoNorm.setOnCheckedChangeListener((v, checked) -> {
				if (checked)
					b.swRgNorm.setChecked(false);
			});

		b.swEqualizer.setOnClickListener((v) -> {
			if (b.swEqualizer.isChecked()) {
				new EQ().show(this, equ, (s) -> {
					equ = s;
				});
			}
		});
	}

	private void play_load() {
		b.sbPlayAutoSkip.setProgress(auto_skip_progress(core.play.auto_skip_head.val));
		b.eAutoSkip.setText(core.play.auto_skip_head.str());
		b.sbPlayAutoSkipTail.setProgress(auto_skip_progress(core.play.auto_skip_tail.val));
		b.eAutoSkipTail.setText(core.play.auto_skip_tail.str());
		b.swEqualizer.setChecked(core.play.equalizer_enabled);
		equ = core.play.equalizer;
	}

	private void play_save() {
		core.setts.set_codepage(b.spCodepage.getSelectedItemPosition());
		core.play.auto_skip_head_set(b.eAutoSkip.getText().toString());
		core.play.auto_skip_tail_set(b.eAutoSkipTail.getText().toString());
		core.play.equalizer_enabled = b.swEqualizer.isChecked();
		core.play.equalizer = equ;
		core.play.normalize();
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

	private void list_load() {
		Queue queue = core.queue();
		b.swRandom.setChecked(queue.flags_test(Phiola.QC_RANDOM));
		b.swRepeat.setChecked(queue.flags_test(Phiola.QC_REPEAT));
		b.swListAddRmOnNext.setChecked(queue.flags_test(Queue.F_MOVE_ON_NEXT));
		b.swListRmOnNext.setChecked(queue.flags_test(Queue.F_RM_ON_NEXT));
		b.swListRmOnErr.setChecked(queue.flags_test(Phiola.QC_REMOVE_ON_ERROR));
		b.swRgNorm.setChecked(queue.flags_test(Phiola.QC_RG_NORM));
		b.swAutoNorm.setChecked(queue.flags_test(Phiola.QC_AUTO_NORM));
		b.sbPlayAutoStop.setProgress(auto_stop_progress(queue.auto_stop_value_min));
		b.eAutoStop.setText(core.int_to_str(queue.auto_stop_value_min));
	}

	private void list_save() {
		int f = 0;
		f |= (b.swRandom.isChecked()) ? Phiola.QC_RANDOM : 0;
		f |= (b.swRepeat.isChecked()) ? Phiola.QC_REPEAT : 0;
		f |= (b.swListAddRmOnNext.isChecked()) ? Queue.F_MOVE_ON_NEXT : 0;
		f |= (b.swListRmOnNext.isChecked()) ? Queue.F_RM_ON_NEXT : 0;
		f |= (b.swListRmOnErr.isChecked()) ? Phiola.QC_REMOVE_ON_ERROR : 0;
		f |= (b.swRgNorm.isChecked()) ? Phiola.QC_RG_NORM : 0;
		f |= (b.swAutoNorm.isChecked()) ? Phiola.QC_AUTO_NORM : 0;
		core.queue().flags_set(Queue.F_ALL, f);

		core.queue().auto_stop_value_min = core.str_to_uint(b.eAutoStop.getText().toString(), 0);
	}

	private void oper_load() {
		b.eDataDir.setText(core.setts.pub_data_dir);
		b.eTrashDir.setText(core.setts.trash_dir);
		b.swFileDel.setChecked(core.setts.file_del);
		b.eLibraryDir.setText(core.setts.library_dir);
		b.swDeprecatedMods.setChecked(core.setts.deprecated_mods);
	}

	private void oper_save() {
		String s = b.eDataDir.getText().toString();
		if (s.isEmpty())
			s = core.storage_path + "/phiola";
		core.setts.pub_data_dir = s;

		core.setts.svc_notification_disable = b.swSvcNotifDisable.isChecked();
		core.setts.trash_dir = b.eTrashDir.getText().toString();
		core.setts.file_del = b.swFileDel.isChecked();
		core.setts.library_dir = b.eLibraryDir.getText().toString();
		core.setts.deprecated_mods = b.swDeprecatedMods.isChecked();
	}

	static final int
		RECBTN_REC_MIC = 0,
		RECBTN_PL_MARKER = 2,
		RECBTN_REC_HIDE = 3;

	private void load() {
		// Interface
		b.swDark.setChecked(core.gui().theme == GUI.THM_DARK);
		color = core.gui().main_color;
		b.swShowFilter.setChecked(core.gui().filter_hide);
		b.spRecBtn.setAdapter(spinner_adapter(new String[] {
				"Record From Mic",
				"Playback Marker Set/Jump",
				"Hide Button",
			}));
		int rm = core.gui().record_mode;
		b.spRecBtn.setSelection(
			(core.gui().playback_marker_show) ? RECBTN_PL_MARKER
			: (rm == GUI.RECMODE_HIDE) ? RECBTN_REC_HIDE
			: RECBTN_REC_MIC);
		b.swSvcNotifDisable.setChecked(core.setts.svc_notification_disable);
		b.swUiInfoInTitle.setChecked(core.gui().ainfo_in_title);

		list_load();

		// Playback
		b.spCodepage.setAdapter(spinner_adapter(core.setts.code_pages));
		b.spCodepage.setSelection(core.setts.codepage_index);

		play_load();
		oper_load();
		rec_load();
	}

	private void save() {
		// Interface
		int i = GUI.THM_DEF;
		if (b.swDark.isChecked())
			i = GUI.THM_DARK;
		core.gui().theme = i;

		core.gui().main_color = color;
		core.gui().filter_hide = b.swShowFilter.isChecked();
		i = b.spRecBtn.getSelectedItemPosition();
		core.gui().record_mode =
			(i == RECBTN_REC_HIDE) ? GUI.RECMODE_HIDE
			: GUI.RECMODE_MIC;
		core.gui().playback_marker_show = (i == RECBTN_PL_MARKER);
		core.gui().ainfo_in_title = b.swUiInfoInTitle.isChecked();

		list_save();
		play_save();
		oper_save();
		rec_save();
		core.queue().conf_normalize();
		core.setts.normalize();
		core.conf_apply();
	}

	// 20%..1%; 0; 10sec..200sec by 10
	private static int auto_skip_value(int progress) {
		progress -= 20;
		if (progress <= 0)
			return progress;
		return progress * 10;
	}
	private static int auto_skip_progress(int n) {
		if (n < 0)
			return 20 - -n;

		if (n <= 200)
			return 20 + n / 10;
		return 20;
	}

	// 5m..6h by 5m
	private static int auto_stop_value(int progress) { return 5 + progress * 5; }
	private static int auto_stop_progress(int min) { return (min - 5) / 5; }

	private static int rec_format_index(String s) {
		return Arrays.asList(RecSettings.rec_formats).indexOf(s);
	}

	private static int rec_sample_format_i(int sf) {
		return Arrays.asList(RecSettings.sample_formats).indexOf(sf);
	}

	private static int rec_src_preset_index(String s) {
		return Arrays.asList(RecSettings.rec_src_presets).indexOf(s);
	}

	// Default; 8000..192000 by 1000
	private static int rec_rate_value(int progress) { return 8000 + (progress - 1) * 1000; }
	private static int rec_rate_progress(int rate) { return 1 + (rate - 8000) / 1000; }

	// 8..256 by 8
	private static int rec_bitrate_value(int progress) { return 8 + (progress) * 8; }
	private static int rec_bitrate_progress(int value) { return (value - 8) / 8; }

	// 10m..12h by 10m
	private static int rec_until_value(int progress) { return 10*60 + (progress) * 10*60; }
	private static int rec_until_progress(int value) { return (value - 10*60) / (10*60); }

	private static String time_str(int sec) { return String.format("%02d:%02d:%02d", sec / 3600, (sec / 60) % 60, sec % 60); }

	/** Convert string like '[[HH:]MM:]SS' to seconds */
	private static int time_sec(String s) {
		int sec = -1, min = 0, hr = 0;

		String[] v = s.split(":");
		if (v.length >= 1)
			sec = Util.str_to_uint(v[v.length - 1], -1);
		if (v.length >= 2)
			min = Util.str_to_uint(v[v.length - 2], -1);
		if (v.length >= 3)
			hr = Util.str_to_uint(v[v.length - 3], -1);
		if (v.length >= 4)
			return -1;

		if (sec < 0 || min < 0 || hr < 0)
			return -1;
		return hr*3600 + min*60 + sec;
	}

	private void rec_init() {
		SeekBar.OnSeekBarChangeListener sbcl = new SeekBar.OnSeekBarChangeListener() {
			@Override
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				if (fromUser) {
					String s;
					switch (seekBar.getId()) {
					case R.id.sbRecRate:
						b.eRecRate.setText((progress != 0) ? Util.int_to_str(rec_rate_value(progress)) : "Default");
						break;

					case R.id.sbRecBitrate:
						b.eRecBitrate.setText(String.format("%d", rec_bitrate_value(progress)));
						break;

					case R.id.sbRecUntil:
						b.eRecUntil.setText(time_str(rec_until_value(progress)));
						break;

					case R.id.sbRecGain:
						s = (progress > 0) ? "+" : "";
						b.eRecGain.setText(s + core.int_to_str(progress));
						break;
					}
				}
			}

			@Override
			public void onStartTrackingTouch(SeekBar seekBar) {}

			@Override
			public void onStopTrackingTouch(SeekBar seekBar) {}
		};
		b.sbRecRate.setOnSeekBarChangeListener(sbcl);
		b.sbRecBitrate.setOnSeekBarChangeListener(sbcl);
		b.sbRecUntil.setOnSeekBarChangeListener(sbcl);
		b.sbRecGain.setOnSeekBarChangeListener(sbcl);

		b.eRecDir.setOnClickListener(v -> explorer.show(b.eRecDir, 0));
		b.spRecSource.setAdapter(spinner_adapter(RecSettings.rec_src_presets));
		b.spRecChannels.setAdapter(spinner_adapter(new String[] {
				"Default",
				"1 (Mono)",
				"2 (Stereo)",
			}));

		b.sbRecRate.setMax(rec_rate_progress(192000));
		b.sbRecBitrate.setMax(rec_bitrate_progress(256));
		b.sbRecUntil.setMax(rec_until_progress(12*3600));
		b.spRecEnc.setAdapter(spinner_adapter(RecSettings.rec_formats));
		b.sbRecGain.setMax(24); // 0..24
	}

	private void rec_load() {
		b.swRecLongclick.setChecked(core.rec.rec_longclick);
		b.eRecDir.setText(core.rec.rec_path);
		b.eRecName.setText(core.rec.rec_name_template);
		b.spRecChannels.setSelection(core.rec.rec_channels);
		if (core.rec.rec_rate != 0) {
			b.eRecRate.setText(core.int_to_str(core.rec.rec_rate));
			b.sbRecRate.setProgress(rec_rate_progress(core.rec.rec_rate));
		}
		b.spRecEnc.setSelection(rec_format_index(core.rec.rec_enc));
		b.sbRecBitrate.setProgress(rec_bitrate_progress(core.rec.rec_bitrate));
		b.eRecBitrate.setText(Integer.toString(core.rec.rec_bitrate));

		b.spInFmt.setAdapter(spinner_adapter(RecSettings.sample_formats_str));
		b.spInFmt.setSelection(rec_sample_format_i(core.rec.rec_input_format));

		b.eRecBufLen.setText(core.int_to_str(core.rec.rec_buf_len_ms));
		b.sbRecUntil.setProgress(rec_until_progress(core.rec.rec_until_sec));
		b.eRecUntil.setText(time_str(core.rec.rec_until_sec));
		b.swRecDanorm.setChecked(core.rec.rec_danorm);
		b.eRecGain.setText(core.float_to_str((float)core.rec.rec_gain_db100 / 100));
		b.sbRecGain.setProgress((int)core.rec.rec_gain_db100 / 100);
		b.swRecExclusive.setChecked(core.rec.rec_exclusive);
		b.spRecSource.setSelection(rec_src_preset_index(core.rec.rec_src_preset));
		b.swRecListAdd.setChecked(core.rec.rec_list_add);
	}

	private void rec_save() {
		core.rec.rec_longclick = b.swRecLongclick.isChecked();
		core.rec.rec_path = b.eRecDir.getText().toString();
		core.rec.rec_name_template = b.eRecName.getText().toString();
		core.rec.rec_bitrate = core.str_to_uint(b.eRecBitrate.getText().toString(), -1);
		core.rec.rec_channels = b.spRecChannels.getSelectedItemPosition();
		core.rec.rec_rate = core.str_to_uint(b.eRecRate.getText().toString(), 0);
		core.rec.rec_enc = RecSettings.rec_formats[b.spRecEnc.getSelectedItemPosition()];
		core.rec.rec_input_format = core.rec.sample_format(RecSettings.sample_formats_str[b.spInFmt.getSelectedItemPosition()]);
		core.rec.rec_buf_len_ms = core.str_to_uint(b.eRecBufLen.getText().toString(), -1);
		core.rec.rec_until_sec = time_sec(b.eRecUntil.getText().toString());
		core.rec.rec_danorm = b.swRecDanorm.isChecked();
		core.rec.rec_gain_db100 = (int)(core.str_to_float(b.eRecGain.getText().toString(), 0) * 100);
		core.rec.rec_exclusive = b.swRecExclusive.isChecked();
		core.rec.rec_src_preset = RecSettings.rec_src_presets[b.spRecSource.getSelectedItemPosition()];
		core.rec.rec_list_add = b.swRecListAdd.isChecked();
		core.rec.normalize();
	}
}
