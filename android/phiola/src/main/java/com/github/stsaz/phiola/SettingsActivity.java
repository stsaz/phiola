/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Build;
import android.os.Bundle;

import android.graphics.PorterDuff;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.SeekBar;

import androidx.appcompat.app.AppCompatActivity;

import com.github.stsaz.phiola.databinding.SettingsBinding;

import java.util.ArrayList;
import java.util.Arrays;

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

	private static void seekbar_color(SeekBar sb, int color) {
		sb.getProgressDrawable().setColorFilter(0xff000000 | color, PorterDuff.Mode.SRC_IN);
		sb.getThumb().setColorFilter(0xff000000 | color, PorterDuff.Mode.SRC_IN);
	}

	private void ui_init() {
		b.sbColorRed.setMax(color_progress(256));
		b.sbColorRed.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						color_modify(0x00ffff, (progress * 3) << 16);
				}
			});
		seekbar_color(b.sbColorRed, 0xff4136);

		b.sbColorGreen.setMax(color_progress(256));
		b.sbColorGreen.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						color_modify(0xff00ff, (progress * 3) << 8);
				}
			});
		seekbar_color(b.sbColorGreen, 0x2ecc40);

		b.sbColorBlue.setMax(color_progress(256));
		b.sbColorBlue.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						color_modify(0xffff00, progress * 3);
				}
			});
		seekbar_color(b.sbColorBlue, 0x0074d9);
	}

	private static final int EQLZ_BANDS = 5;
	private ArrayList<String> eqlz_vals;
	private int eqlz_band, eqlz_freq, eqlz_width, eqlz_gain;

	private void eqlz_enable(boolean enable) {
		b.spEqlzBand.setEnabled(enable);
		b.sbEqlzFreq.setEnabled(enable);
		b.sbEqlzWidth.setEnabled(enable);
		b.sbEqlzGain.setEnabled(enable);
		b.eEqualizer.setEnabled(enable);
	}

	private static int eqlz_freq_value(int progress) { return progress * 100; }
	private static int eqlz_freq_progress(int val) { return val / 100; }
	private static int eqlz_width_value(int progress) { return progress; }
	private static int eqlz_width_progress(int val) { return val; }
	private static int eqlz_gain_value(int progress) { return progress - 120; }
	private static int eqlz_gain_progress(int val) { return val + 120; }

	private void eqlz_init() {
		String[] v = core.play.equalizer.split(" ");
		int n = EQLZ_BANDS * 3;
		eqlz_vals = new ArrayList<>(n);
		eqlz_vals.addAll(Arrays.asList(v));
		for (int i = eqlz_vals.size();  i < n;  i++) {
			eqlz_vals.add("");
		}
		eqlz_show(1);
	}

	private void eqlz_show(int band) {
		if (eqlz_band != 0) {
			String[] v = b.eEqualizer.getText().toString().split(" ");
			if (v.length != 3)
				v = new String[]{"", "", ""};
			eqlz_set(v);
		}

		eqlz_band = band;
		int off = 3 * (band - 1);

		eqlz_freq = Core.str_to_uint(eqlz_vals.get(off + 0), 0);
		b.sbEqlzFreq.setProgress(eqlz_freq_progress(eqlz_freq));

		String s = eqlz_vals.get(off + 1);
		if (s.length() >= 2)
			s = s.substring(0, s.length() - 1); // cut last 'q'
		eqlz_width = (int)(Core.str_to_float(s, 0) * 10);
		b.sbEqlzWidth.setProgress(eqlz_width_progress(eqlz_width));

		eqlz_gain = (int)(Core.str_to_float(eqlz_vals.get(off + 2), 0) * 10);
		b.sbEqlzGain.setProgress(eqlz_gain_progress(eqlz_gain));

		s = String.format("%s %s %s"
			, eqlz_vals.get(off + 0)
			, eqlz_vals.get(off + 1)
			, eqlz_vals.get(off + 2));
		b.eEqualizer.setText(s);
	}

	private void eqlz_changed() {
		String[] v = new String[] {
			String.format("%d", eqlz_freq),
			String.format("%.01fq", (double)eqlz_width / 10),
			String.format("%.01f", (double)eqlz_gain / 10),
		};
		eqlz_set(v);
		String s = String.format("%s %s %s", v[0], v[1], v[2]);
		core.dbglog(TAG, "eqlz_vals band str: '%s'", s);
		b.eEqualizer.setText(s);
	}

	private void eqlz_set(String[] v) {
		int off = 3 * (eqlz_band - 1);
		eqlz_vals.set(off + 0, v[0]);
		eqlz_vals.set(off + 1, v[1]);
		eqlz_vals.set(off + 2, v[2]);
	}

	private String eqlz_commit() {
		StringBuilder sb = new StringBuilder();
		for (String s : eqlz_vals) {
			sb.append(String.format("%s ", s));
		}
		String es = sb.substring(0, sb.length() - 1);
		core.dbglog(TAG, "eqlz_str: '%s'", es);
		return es;
	}

	private void play_init() {
		b.sbPlayAutoSkip.setMax(auto_skip_progress(200));
		b.sbPlayAutoSkip.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eAutoSkip.setText(AutoSkip.user_str(auto_skip_value(progress)));
				}
			});

		b.sbPlayAutoSkipTail.setMax(auto_skip_progress(200));
		b.sbPlayAutoSkipTail.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eAutoSkipTail.setText(AutoSkip.user_str(auto_skip_value(progress)));
				}
			});

		b.sbPlayAutoStop.setMax(auto_stop_progress(600));
		b.sbPlayAutoStop.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eAutoStop.setText(Util.int_to_str(auto_stop_value(progress)));
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

		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, new String[] {
				"Band #1","Band #2","Band #3","Band #4","Band #5",
			});
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.spEqlzBand.setAdapter(adapter);
		b.spEqlzBand.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
				@Override
				public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
					eqlz_show(b.spEqlzBand.getSelectedItemPosition() + 1);
				}
				@Override
				public void onNothingSelected(AdapterView<?> parent) {
				}
			});

		b.swEqualizer.setOnCheckedChangeListener((v, checked) -> { eqlz_enable(checked); });
		b.sbEqlzFreq.setMax(220); // 0..22000 Hz
		b.sbEqlzWidth.setMax(40); // 0..4.0
		b.sbEqlzGain.setMax(120+120); // -12.0..12.0
		b.sbEqlzFreq.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						eqlz_freq = eqlz_freq_value(progress);
						eqlz_changed();
					}
				}
			});
		b.sbEqlzWidth.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						eqlz_width = eqlz_width_value(progress);
						eqlz_changed();
					}
				}
			});
		b.sbEqlzGain.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						eqlz_gain = eqlz_gain_value(progress);
						eqlz_changed();
					}
				}
			});
	}

	private void play_load() {
		b.sbPlayAutoSkip.setProgress(auto_skip_progress(core.play.auto_skip_head.val));
		b.eAutoSkip.setText(core.play.auto_skip_head.str());
		b.sbPlayAutoSkipTail.setProgress(auto_skip_progress(core.play.auto_skip_tail.val));
		b.eAutoSkipTail.setText(core.play.auto_skip_tail.str());
		b.swEqualizer.setChecked(core.play.equalizer_enabled);
		if (!core.play.equalizer_enabled)
			eqlz_enable(false);
		eqlz_init();
	}

	private void play_save() {
		core.setts.set_codepage(b.spCodepage.getSelectedItemPosition());
		core.play.auto_skip_head_set(b.eAutoSkip.getText().toString());
		core.play.auto_skip_tail_set(b.eAutoSkipTail.getText().toString());
		core.play.equalizer_set(b.swEqualizer.isChecked(), eqlz_commit());
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

	private static abstract class SBOnSeekBarChangeListener implements SeekBar.OnSeekBarChangeListener {
		@Override
		public void onStartTrackingTouch(SeekBar seekBar) {}

		@Override
		public void onStopTrackingTouch(SeekBar seekBar) {}
	}

	private void load() {
		// Interface
		b.swDark.setChecked(core.gui().theme == GUI.THM_DARK);
		int n = core.gui().main_color;
		if (n >= 0) {
			b.sbColorRed.setProgress(color_progress((n & 0xff0000) >> 16));
			b.sbColorGreen.setProgress(color_progress((n & 0x00ff00) >> 8));
			b.sbColorBlue.setProgress(color_progress(n & 0x0000ff));
			b.eColor.setText(Util.color_str(n));
		}
		b.swShowfilter.setChecked(core.gui().filter_hide);
		b.swShowrec.setChecked(core.gui().record_hide);
		b.swShowPlaybackMarker.setChecked(core.gui().playback_marker_show);
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
		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, core.setts.code_pages);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.spCodepage.setAdapter(adapter);
		b.spCodepage.setSelection(core.setts.codepage_index);

		play_load();

		// Operation
		b.eDataDir.setText(core.setts.pub_data_dir);
		b.eTrashDir.setText(core.setts.trash_dir);
		b.swFileDel.setChecked(core.setts.file_del);
		b.eLibraryDir.setText(core.setts.library_dir);
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

		core.gui().main_color = Util.color_from_str(b.eColor.getText().toString(), -1);
		core.gui().filter_hide = b.swShowfilter.isChecked();
		core.gui().record_hide = b.swShowrec.isChecked();
		core.gui().playback_marker_show = b.swShowPlaybackMarker.isChecked();
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

		play_save();

		// Operation
		String s = b.eDataDir.getText().toString();
		if (s.isEmpty())
			s = core.storage_path + "/phiola";
		core.setts.pub_data_dir = s;

		core.setts.svc_notification_disable = b.swSvcNotifDisable.isChecked();
		core.setts.trash_dir = b.eTrashDir.getText().toString();
		core.setts.file_del = b.swFileDel.isChecked();
		core.setts.library_dir = b.eLibraryDir.getText().toString();
		core.setts.deprecated_mods = b.swDeprecatedMods.isChecked();

		rec_save();
		core.queue().conf_normalize();
		core.setts.normalize();
		core.phiola.setConfig(core.setts.codepage, core.setts.deprecated_mods);
	}

	private static int color_progress(int value) { return value / 3; }
	private void color_modify(int mask, int value) {
		int n = core.gui().main_color;
		if (n < 0)
			n = 0; // default -> black
		n = (n & mask) | value;
		core.gui().main_color = n;
		b.eColor.setText(Util.color_str(n));
		b.eColor.setBackgroundColor(0xff000000 | n);
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

	private static int auto_stop_value(int progress) { return progress * 6; }
	private static int auto_stop_progress(int min) { return min / 6; }

	private static int rec_format_index(String s) {
		return Arrays.asList(RecSettings.rec_formats).indexOf(s);
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
		b.eRecDir.setOnClickListener(v -> explorer.show(b.eRecDir, 0));

		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, RecSettings.rec_src_presets);
		b.spRecSource.setAdapter(adapter);

		adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, new String[] {
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
							s = Util.int_to_str(rec_rate_value(progress));
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

		b.sbRecUntil.setMax(rec_until_progress(12*3600));
		b.sbRecUntil.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eRecUntil.setText(time_str(rec_until_value(progress)));
				}
			});

		adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, RecSettings.rec_formats);
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

		if (Build.VERSION.SDK_INT < 26) {
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
