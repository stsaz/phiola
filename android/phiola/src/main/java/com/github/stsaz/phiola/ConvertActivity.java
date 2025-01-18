/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;

import android.widget.ArrayAdapter;
import android.widget.SeekBar;

import androidx.appcompat.app.AppCompatActivity;

import com.github.stsaz.phiola.databinding.ConvertBinding;

public class ConvertActivity extends AppCompatActivity {
	private static final String TAG = "phiola.ConvertActivity";
	Core core;
	private VarMenu varmenu;
	private ExplorerMenu explorer;
	private long length_msec;
	private ConvertBinding b;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = ConvertBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		varmenu = new VarMenu(this);
		explorer = new ExplorerMenu(this);
		b.eOutDir.setOnClickListener(v -> explorer.show(b.eOutDir));
		b.eOutName.setOnClickListener(v -> varmenu.show(b.eOutName, new String[]{
				"@filename", "@album", "@artist", "@title", "@tracknumber",
			}));

		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, CoreSettings.conv_format_display);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.spOutExt.setAdapter(adapter);

		b.sbRangeFrom.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						b.eFrom.setText(time_str(time_value(progress)));
					}
				}
			});
		b.bFromSetCur.setOnClickListener((v) -> pos_set_cur(true));

		b.sbRangeUntil.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						String s = "";
						if (!(progress == 0 || progress == 100))
							s = time_str(time_value(progress));
						b.eUntil.setText(s);
					}
				}
			});
		b.bUntilSetCur.setOnClickListener((v) -> pos_set_cur(false));

		adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, CoreSettings.conv_sample_formats_str);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.spSampleFormat.setAdapter(adapter);

		b.sbAacQ.setMax(aac_q_progress(5));
		b.sbAacQ.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eAacQ.setText(aac_q_write(aac_q_value(progress)));
				}
			});

		b.sbOpusQ.setMax(opus_q_progress(510));
		b.sbOpusQ.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eOpusQ.setText(core.int_to_str(opus_q_value(progress)));
				}
			});

		b.sbVorbisQ.setMax(vorbis_q_progress(10));
		b.sbVorbisQ.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eVorbisQ.setText(core.int_to_str(vorbis_q_value(progress)));
				}
			});

		b.swCopy.setOnCheckedChangeListener((v, checked) -> {
				b.spSampleFormat.setEnabled(!checked);
				b.eSampleRate.setEnabled(!checked);

				b.sbAacQ.setEnabled(!checked);
				b.eAacQ.setEnabled(!checked);

				b.sbOpusQ.setEnabled(!checked);
				b.eOpusQ.setEnabled(!checked);

				b.sbVorbisQ.setEnabled(!checked);
				b.eVorbisQ.setEnabled(!checked);
			});

		b.bStart.setOnClickListener((v) -> convert());

		core = Core.getInstance();
		load();

		String iname = getIntent().getStringExtra("iname");
		b.eInName.setEnabled(false);
		if (iname != null) {
			b.eInName.setText(iname);
			length_msec = getIntent().getLongExtra("length", 0);
		} else {
			b.eInName.setText("(multiple files)");
			b.bFromSetCur.setEnabled(false);
			b.bUntilSetCur.setEnabled(false);
			length_msec = 5*60*1000;
		}
	}

	private static abstract class SBOnSeekBarChangeListener implements SeekBar.OnSeekBarChangeListener {
		@Override
		public void onStartTrackingTouch(SeekBar seekBar) {}

		@Override
		public void onStopTrackingTouch(SeekBar seekBar) {}
	}

	private int time_progress(long sec) {
		if (length_msec == 0)
			return 0;
		return (int)(sec * 100 / (length_msec/1000));
	}
	private long time_value(int progress) {
		return length_msec/1000 * progress / 100;
	}
	private String time_str(long sec) {
		return String.format("%d:%02d", sec / 60, sec % 60);
	}

	private int conv_format_index(String name) {
		for (int i = 0; i < CoreSettings.conv_formats.length; i++) {
			if (CoreSettings.conv_formats[i].equals(name))
				return i;
		}
		return -1;
	}

	// 16..800 by 16; 1..5
	private static int aac_q_value(int progress) {
		if (progress > (800 - 16) / 16)
			return progress - (800 - 16) / 16;
		return 16 + progress * 16;
	}
	private static int aac_q_progress(int q) {
		if (q <= 5)
			return (800 - 16) / 16 + q;
		return (q - 16) / 16;
	}
	private String aac_q_write(int val) {
		if (val <= 5)
			return "VBR:" + core.int_to_str(val);
		return core.int_to_str(val);
	}
	private int aac_q_read(String s) {
		if (s.indexOf("VBR:") == 0)
			s = s.substring(4);
		return core.str_to_uint(s, 0);
	}

	// 16..496 by 16
	private static int opus_q_value(int progress) { return 16 + progress * 16; }
	private static int opus_q_progress(int q) { return (q - 16) / 16; }

	// 1..10
	private static int vorbis_q_value(int progress) { return 1 + progress; }
	private static int vorbis_q_progress(int q) { return q - 1; }

	private void load() {
		b.eOutDir.setText(core.setts.conv_out_dir);
		b.eOutName.setText(core.setts.conv_out_name);
		b.spOutExt.setSelection(conv_format_index(core.setts.conv_format));

		b.sbAacQ.setProgress(aac_q_progress(core.setts.conv_aac_quality));
		b.eAacQ.setText(core.int_to_str(core.setts.conv_aac_quality));

		b.sbOpusQ.setProgress(opus_q_progress(core.setts.conv_opus_quality));
		b.eOpusQ.setText(core.int_to_str(core.setts.conv_opus_quality));

		b.sbVorbisQ.setProgress(vorbis_q_progress(core.setts.conv_vorbis_quality));
		b.eVorbisQ.setText(core.int_to_str(core.setts.conv_vorbis_quality));

		b.swCopy.setChecked(core.setts.conv_copy);

		b.swPreserveDate.setChecked(core.setts.conv_file_date_preserve);
		b.swPlAdd.setChecked(core.setts.conv_new_add_list);
	}

	private void save() {
		String s = b.eOutDir.getText().toString();
		if (s.isEmpty()) {
			s = "@filepath";
			b.eOutDir.setText(s);
		}
		core.setts.conv_out_dir = s;

		s = b.eOutName.getText().toString();
		if (s.isEmpty()) {
			s = "@filename";
			b.eOutName.setText(s);
		}
		core.setts.conv_out_name = s;

		core.setts.conv_format = CoreSettings.conv_formats[b.spOutExt.getSelectedItemPosition()];
		core.setts.conv_copy = b.swCopy.isChecked();

		int v = aac_q_read(b.eAacQ.getText().toString());
		if (v != 0)
			core.setts.conv_aac_quality = v;

		v = core.str_to_uint(b.eOpusQ.getText().toString(), 0);
		if (v != 0)
			core.setts.conv_opus_quality = v;

		v = core.str_to_uint(b.eVorbisQ.getText().toString(), 0);
		if (v != 0)
			core.setts.conv_vorbis_quality = v;

		core.setts.conv_file_date_preserve = b.swPreserveDate.isChecked();
		core.setts.conv_new_add_list = b.swPlAdd.isChecked();
	}

	@Override
	protected void onDestroy() {
		save();
		core.setts.normalize_convert();
		core.unref();
		super.onDestroy();
	}

	/** Set 'from/until' position equal to the current playing position */
	private void pos_set_cur(boolean from) {
		long sec = core.track.curpos_msec() / 1000;
		String s = time_str(sec);
		if (from) {
			b.sbRangeFrom.setProgress(time_progress(sec));
			b.eFrom.setText(s);
		} else {
			b.sbRangeUntil.setProgress(time_progress(sec));
			b.eUntil.setText(s);
		}
	}

	String iname, oname;
	private void convert() {
		b.bStart.setEnabled(false);

		save();

		Phiola.ConvertParams p = new Phiola.ConvertParams();
		p.from_msec = b.eFrom.getText().toString();
		p.to_msec = b.eUntil.getText().toString();
		p.tags = b.eTags.getText().toString();
		p.sample_format = CoreSettings.conv_sample_formats[b.spSampleFormat.getSelectedItemPosition()];
		p.sample_rate = core.str_to_uint(b.eSampleRate.getText().toString(), 0);
		p.aac_quality = core.setts.conv_aac_quality;
		p.opus_quality = core.setts.conv_opus_quality;
		p.vorbis_quality = (core.setts.conv_vorbis_quality + 1) * 10;

		int iformat = b.spOutExt.getSelectedItemPosition();
		p.format = CoreSettings.conv_encoders[iformat];

		if (core.setts.conv_copy)
			p.flags |= Phiola.ConvertParams.COF_COPY;
		if (core.setts.conv_file_date_preserve)
			p.flags |= Phiola.ConvertParams.COF_DATE_PRESERVE;
		if (b.swOutOverwrite.isChecked() && b.swOutOverwriteConfirm.isChecked())
			p.flags |= Phiola.ConvertParams.COF_OVERWRITE;

		if (core.setts.conv_new_add_list) {
			p.q_add_remove = getIntent().getLongExtra("current_list_id", 0);
			p.flags |= Phiola.ConvertParams.COF_ADD;
		}

		if (b.swTrashOrig.isChecked()) {
			p.trash_dir_rel = core.setts.trash_dir;
			p.q_add_remove = getIntent().getLongExtra("current_list_id", 0);
			p.q_pos = getIntent().getIntExtra("active_track_pos", -1);
		}

		p.out_name = String.format("%s/%s.%s"
			, core.setts.conv_out_dir, core.setts.conv_out_name, CoreSettings.conv_extensions[iformat]);

		String r = core.queue().convert_begin(p);
		if (r != null) {
			b.bStart.setEnabled(true);
			b.lResult.setText(r);
			return;
		}

		setResult(RESULT_OK);
		finish();
	}
}
