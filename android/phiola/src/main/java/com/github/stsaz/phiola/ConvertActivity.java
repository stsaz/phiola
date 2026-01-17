/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;

import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.SeekBar;

import androidx.appcompat.app.AppCompatActivity;

import com.github.stsaz.phiola.databinding.ConvertBinding;

import java.util.Arrays;

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

		core = Core.getInstance();
		core.gui().on_activity_show(this);

		init();
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

	@Override
	protected void onDestroy() {
		save();
		core.convert.normalize();
		core.unref();
		super.onDestroy();
	}

	private static abstract class SBOnSeekBarChangeListener implements SeekBar.OnSeekBarChangeListener {
		@Override
		public void onStartTrackingTouch(SeekBar seekBar) {}

		@Override
		public void onStopTrackingTouch(SeekBar seekBar) {}
	}

	private void init() {
		varmenu = new VarMenu(this);
		explorer = new ExplorerMenu(core, this);
		b.eOutDir.setOnClickListener(v -> explorer.show(b.eOutDir, 0));
		b.eOutName.setOnClickListener(v -> varmenu.show(b.eOutName, new String[]{
				"@filename", "@album", "@artist", "@title", "@tracknumber",
			}));

		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, ConvertSettings.conv_format_display);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.spOutExt.setAdapter(adapter);
		b.spOutExt.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
				@Override
				public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
					enc_setts_enable(position);
				}
				@Override
				public void onNothingSelected(AdapterView<?> parent) {}
			});

		b.sbRangeFrom.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eFrom.setText(time_str(time_value(progress)));
				}
			});
		b.bFromSetCur.setOnClickListener((v) -> pos_set_cur(true));

		b.sbRangeUntil.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eUntil.setText((progress > 0 && progress < 100) ? time_str(time_value(progress)) : "");
				}
			});
		b.bUntilSetCur.setOnClickListener((v) -> pos_set_cur(false));

		adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, ConvertSettings.conv_sample_formats_str);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.spSampleFormat.setAdapter(adapter);

		b.sbAACQ.setMax(aac_q_progress(5));
		b.sbAACQ.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eAACQ.setText(aac_q_write(aac_q_value(progress)));
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

		b.sbMp3Q.setMax(9);
		b.sbMp3Q.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser)
						b.eMp3Q.setText(core.int_to_str(mp3_q_value(progress)));
				}
			});

		b.swCopy.setOnCheckedChangeListener((v, checked) -> {
				b.spSampleFormat.setEnabled(!checked);
				b.eSampleRate.setEnabled(!checked);
				enc_setts_enable((!checked) ? b.spOutExt.getSelectedItemPosition() : -1);
			});

		b.bStart.setOnClickListener((v) -> convert());
	}

	private void enc_setts_enable(int iformat) {
		boolean[] v = new boolean[3];
		if (iformat >= 0) {
			switch (ConvertSettings.conv_encoders[iformat]) {
			case Phiola.AF_AAC_LC:
			case Phiola.AF_AAC_HE:
				v[0] = true;  break;
			case Phiola.AF_OPUS:
				v[1] = true;  break;
			case Phiola.AF_MP3:
				v[2] = true;  break;
			}
		}
		b.sbAACQ.setEnabled(v[0]);
		b.eAACQ.setEnabled(v[0]);
		b.sbOpusQ.setEnabled(v[1]);
		b.eOpusQ.setEnabled(v[1]);
		b.sbMp3Q.setEnabled(v[2]);
		b.eMp3Q.setEnabled(v[2]);
	}

	private int time_progress(long sec) {
		return (length_msec != 0) ? (int)(sec * 100 / (length_msec/1000)) : 0;
	}
	private long time_value(int progress) {
		return length_msec/1000 * progress / 100;
	}
	private String time_str(long sec) {
		return String.format("%d:%02d", sec / 60, sec % 60);
	}

	private int conv_format_index(String name) {
		return Arrays.asList(ConvertSettings.conv_formats).indexOf(name);
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

	// 9..0
	private static int mp3_q_value(int progress) { return 9 - progress; }
	private static int mp3_q_progress(int q) { return 9 - q; }

	private void load() {
		b.eOutDir.setText(core.convert.conv_out_dir);
		b.eOutName.setText(core.convert.conv_out_name);
		b.spOutExt.setSelection(conv_format_index(core.convert.conv_format));

		b.sbAACQ.setProgress(aac_q_progress(core.convert.conv_aac_quality));
		b.eAACQ.setText(core.int_to_str(core.convert.conv_aac_quality));

		b.sbOpusQ.setProgress(opus_q_progress(core.convert.conv_opus_quality));
		b.eOpusQ.setText(core.int_to_str(core.convert.conv_opus_quality));

		b.sbMp3Q.setProgress(mp3_q_progress(core.convert.conv_mp3_quality));
		b.eMp3Q.setText(core.int_to_str(core.convert.conv_mp3_quality));

		b.swCopy.setChecked(core.convert.conv_copy);

		b.swPreserveDate.setChecked(core.convert.conv_file_date_preserve);
		b.swPlAdd.setChecked(core.convert.conv_new_add_list);
	}

	private void save() {
		String s = b.eOutDir.getText().toString();
		if (s.isEmpty()) {
			s = "@filepath";
			b.eOutDir.setText(s);
		}
		core.convert.conv_out_dir = s;

		s = b.eOutName.getText().toString();
		if (s.isEmpty()) {
			s = "@filename";
			b.eOutName.setText(s);
		}
		core.convert.conv_out_name = s;

		core.convert.conv_format = ConvertSettings.conv_formats[b.spOutExt.getSelectedItemPosition()];
		core.convert.conv_copy = b.swCopy.isChecked();

		int v = aac_q_read(b.eAACQ.getText().toString());
		if (v != 0)
			core.convert.conv_aac_quality = v;

		v = core.str_to_uint(b.eOpusQ.getText().toString(), 0);
		if (v != 0)
			core.convert.conv_opus_quality = v;

		v = core.str_to_uint(b.eMp3Q.getText().toString(), -1);
		if (v >= 0)
			core.convert.conv_mp3_quality = v;

		core.convert.conv_file_date_preserve = b.swPreserveDate.isChecked();
		core.convert.conv_new_add_list = b.swPlAdd.isChecked();
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

	private void convert() {
		b.bStart.setEnabled(false);

		save();

		Phiola.ConvertParams p = new Phiola.ConvertParams();
		p.from_msec = b.eFrom.getText().toString();
		p.to_msec = b.eUntil.getText().toString();
		p.tags = b.eTags.getText().toString();
		p.sample_format = ConvertSettings.conv_sample_formats[b.spSampleFormat.getSelectedItemPosition()];
		p.sample_rate = core.str_to_uint(b.eSampleRate.getText().toString(), 0);
		p.aac_quality = core.convert.conv_aac_quality;
		p.opus_quality = core.convert.conv_opus_quality;
		p.mp3_quality = core.convert.conv_mp3_quality;

		int iformat = b.spOutExt.getSelectedItemPosition();
		p.format = ConvertSettings.conv_encoders[iformat];

		if (core.convert.conv_copy)
			p.flags |= Phiola.ConvertParams.COF_COPY;
		if (core.convert.conv_file_date_preserve)
			p.flags |= Phiola.ConvertParams.COF_DATE_PRESERVE;
		if (b.swOutOverwrite.isChecked() && b.swOutOverwriteConfirm.isChecked())
			p.flags |= Phiola.ConvertParams.COF_OVERWRITE;

		if (core.convert.conv_new_add_list) {
			p.q_add_remove = getIntent().getLongExtra("current_list_id", 0);
			p.flags |= Phiola.ConvertParams.COF_ADD;
		}

		if (b.swTrashOrig.isChecked()) {
			p.trash_dir_rel = core.setts.trash_dir;
			p.q_add_remove = getIntent().getLongExtra("current_list_id", 0);
			p.q_pos = getIntent().getIntExtra("active_track_pos", -1);
		}

		p.out_name = String.format("%s/%s.%s"
			, core.convert.conv_out_dir, core.convert.conv_out_name, ConvertSettings.conv_extensions[iformat]);

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
