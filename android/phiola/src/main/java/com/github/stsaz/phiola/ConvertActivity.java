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
	private long length_msec;
	private int qu_cur_pos;
	private ConvertBinding b;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = ConvertBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item
			, CoreSettings.conv_extensions);
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

		b.sbAacQ.setMax(aac_q_progress(5));
		b.sbAacQ.setOnSeekBarChangeListener(new SBOnSeekBarChangeListener() {
				@Override
				public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
					if (fromUser) {
						int val = aac_q_value(progress);
						String s;
						if (val <= 5)
							s = "VBR:" + core.int_to_str(val);
						else
							s = core.int_to_str(val);
						b.eAacQ.setText(s);
					}
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

		qu_cur_pos = core.queue().cur();
		String iname = getIntent().getStringExtra("iname");
		b.eInName.setText(iname);

		length_msec = getIntent().getLongExtra("length", 0);

		int sl = iname.lastIndexOf('/');
		if (sl < 0)
			sl = 0;
		else
			sl++;
		b.eOutDir.setText(iname.substring(0, sl));

		int pos = iname.lastIndexOf('.');
		if (pos < 0)
			pos = 0;
		b.eOutName.setText(iname.substring(sl, pos));
	}

	private static abstract class SBOnSeekBarChangeListener implements SeekBar.OnSeekBarChangeListener {
		@Override
		public void onStartTrackingTouch(SeekBar seekBar) {}

		@Override
		public void onStopTrackingTouch(SeekBar seekBar) {}
	}

	private int time_progress(long sec) {
		return (int)(sec * 100 / (length_msec/1000));
	}
	private long time_value(int progress) {
		return length_msec/1000 * progress / 100;
	}
	private String time_str(long sec) {
		return String.format("%d:%02d", sec / 60, sec % 60);
	}

	private int conv_extension(String s) {
		for (int i = 0; i < CoreSettings.conv_extensions.length; i++) {
			if (CoreSettings.conv_extensions[i].equals(s))
				return i;
		}
		return -1;
	}

	// 8..800 by 8; 1..5
	private static int aac_q_value(int progress) {
		if (progress > (800 - 8) / 8)
			return progress - (800 - 8) / 8;
		return 8 + progress * 8;
	}
	private static int aac_q_progress(int q) {
		if (q <= 5)
			return (800 - 8) / 8 + q;
		return (q - 8) / 8;
	}

	// 8..504 by 8
	private static int opus_q_value(int progress) { return 8 + progress * 8; }
	private static int opus_q_progress(int q) { return (q - 8) / 8; }

	// 1..10
	private static int vorbis_q_value(int progress) { return 1 + progress; }
	private static int vorbis_q_progress(int q) { return q - 1; }

	private void load() {
		b.spOutExt.setSelection(conv_extension(core.setts.conv_outext));

		b.sbAacQ.setProgress(aac_q_progress(core.setts.conv_aac_quality));
		b.eAacQ.setText(core.int_to_str(core.setts.conv_aac_quality));

		b.sbOpusQ.setProgress((core.setts.conv_opus_quality - 8) / 8);
		b.eOpusQ.setText(core.int_to_str(core.setts.conv_opus_quality));

		b.sbVorbisQ.setProgress(vorbis_q_progress(core.setts.conv_vorbis_quality));
		b.eVorbisQ.setText(core.int_to_str(core.setts.conv_vorbis_quality));

		b.swCopy.setChecked(core.setts.conv_copy);

		b.swPreserveDate.setChecked(core.setts.conv_file_date_preserve);
		b.swPlAdd.setChecked(core.setts.conv_new_add_list);
	}

	private void save() {
		core.setts.conv_outext = CoreSettings.conv_extensions[b.spOutExt.getSelectedItemPosition()];
		core.setts.conv_copy = b.swCopy.isChecked();

		String s = b.eAacQ.getText().toString();
		if (s.indexOf("VBR:") == 0)
			s = s.substring(4);
		int v = core.str_to_uint(s, 0);
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
		b.lResult.setText(R.string.conv_working);

		Phiola.ConvertParams p = new Phiola.ConvertParams();
		p.from_msec = b.eFrom.getText().toString();
		p.to_msec = b.eUntil.getText().toString();
		p.copy = b.swCopy.isChecked();
		p.sample_rate = core.str_to_uint(b.eSampleRate.getText().toString(), 0);
		p.aac_quality = core.str_to_uint(b.eAacQ.getText().toString(), 0);
		p.opus_quality = core.str_to_uint(b.eOpusQ.getText().toString(), 0);

		int q = core.str_to_uint(b.eVorbisQ.getText().toString(), 0);
		if (q != 0)
			p.vorbis_quality = (q + 1) * 10;

		if (b.swPreserveDate.isChecked())
			p.flags |= Phiola.ConvertParams.F_DATE_PRESERVE;
		if (false)
			p.flags |= Phiola.ConvertParams.F_OVERWRITE;

		iname = b.eInName.getText().toString();
		oname = String.format("%s/%s.%s"
			, b.eOutDir.getText().toString()
			, b.eOutName.getText().toString()
			, CoreSettings.conv_extensions[b.spOutExt.getSelectedItemPosition()]);
		core.phiola.convert(iname, oname, p,
				(result) -> {
					core.tq.post(() -> {
						convert_done(result);
					});
				}
			);
	}

	private void convert_done(String result) {
		boolean ok = result.isEmpty();
		if (ok)
			b.lResult.setText(R.string.conv_done);
		else
			b.lResult.setText(result);

		if (ok && b.swPlAdd.isChecked()) {
			core.queue().add(oname);
		}

		if (ok && b.swTrashOrig.isChecked() && !iname.equals(oname)) {
			trash_input_file(iname);
		}
		b.bStart.setEnabled(true);
	}

	private void trash_input_file(String iname) {
		String e = core.util.trash(core.setts.trash_dir, iname);
		if (!e.isEmpty()) {
			core.errlog(TAG, "Can't trash file %s: %s", iname, e);
			return;
		}

		if (qu_cur_pos == -1) return;

		String url = core.queue().get(qu_cur_pos);
		if (url.equals(iname))
			core.queue().remove(qu_cur_pos);
		qu_cur_pos = -1;
	}
}
