/** phiola/Android
2026, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.Spinner;

import java.util.ArrayList;
import java.util.Arrays;

class EQBand {
	int type, freq, width, gain;

	static final int
			LOW_SHELF = 1,
			HIGH_SHELF = 3;

	EQBand(int t) {
		this.type = t;
	}

	boolean shelf() {
		return (this.type & 1) != 0;
	}

	void parse(String s) {
		this.freq = 0;
		this.width = 0;
		this.gain = 0;

		String[] v = s.split(" ");
		for (int i = 0; i + 1 < v.length; i += 2) {
			if (v[i].equals("f") && !shelf()) {
				this.freq = Core.str_to_uint(v[i + 1], 0);

			} else if (v[i].equals("w") && !shelf()) {
				String w = v[i + 1];
				if (w.length() > 1 && w.charAt(w.length() - 1) == 'q') {
					w = w.substring(0, w.length() - 1); // cut last 'q'
					this.width = (int) (Core.str_to_float(w, 0) * 10);
				}

			} else if (v[i].equals("g")) {
				this.gain = (int) (Core.str_to_float(v[i + 1], 0) * 10);
			}
		}
	}

	// 20..20000 Hz
	void freq_set(int progress) {
		freq = (int) ((20000 - 20) * Math.pow((double) (progress + 1) / 100, 3) + 20);
	}

	int freq_progress() {
		return 0;
	}

	void gain_set(int progress) {
		gain = (progress * 5) - 120;
	}

	int gain_progress() {
		return (gain + 120) / 5;
	}

	String str() {
		if (shelf()) {
			return String.format("t %s g %.01f"
					, (this.type == 1) ? "bass" : "treble", (double) this.gain / 10);
		}
		return String.format("f %d w %.01fq g %.01f"
				, this.freq, (double) this.width / 10, (double) this.gain / 10);
	}
}

class EQSet {
	static final int BANDS = 5;
	private ArrayList<String> parts;
	int band;

	// "t T f F w W g G,..."
	void init(String conf) {
		String[] v = conf.split(",");
		parts = new ArrayList<>(BANDS);
		parts.addAll(Arrays.asList(v));
		for (int i = parts.size(); i < BANDS; i++) {
			parts.add("");
		}
	}

	String commit() {
		StringBuilder sb = new StringBuilder();
		for (String s : parts) {
			sb.append(String.format("%s,", s));
		}
		return sb.substring(0, sb.length() - 1);
	}

	String current() {
		return parts.get(band - 1);
	}

	void current_set(String s) {
		parts.set(band - 1, s);
	}

	EQBand select(int i) {
		band = i;
		int t = (band == 1) ? EQBand.LOW_SHELF
				: (band == BANDS) ? EQBand.HIGH_SHELF
				: 0;
		EQBand b = new EQBand(t);
		b.parse(current());
		return b;
	}
}

public class EQ {
	private EQSet eqset;
	private EQBand eqband;
	private Spinner spBand;
	private SeekBar sbFreq, sbWidth, sbGain;
	private EditText eEQ;

	interface Confirmed {
		void f(String eq);
	}

	private Dialog create(Context ctx, Confirmed cb) {
		View v = LayoutInflater.from(ctx).inflate(R.layout.equalizer, null);
		spBand = v.findViewById(R.id.spBand);
		sbFreq = v.findViewById(R.id.sbFreq);
		sbGain = v.findViewById(R.id.sbGain);
		sbWidth = v.findViewById(R.id.sbWidth);
		eEQ = v.findViewById(R.id.eEQ);
		spBand.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
			@Override
			public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
				eqset.current_set(eEQ.getText().toString());
				switch_band(position + 1);
			}

			@Override
			public void onNothingSelected(AdapterView<?> parent) {
			}
		});

		SeekBar.OnSeekBarChangeListener sbcl = new SeekBar.OnSeekBarChangeListener() {
			@Override
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				if (fromUser) {
					switch (seekBar.getId()) {
						case R.id.sbFreq:
							eqband.freq_set(progress);  break;

						case R.id.sbWidth:
							eqband.width = progress;  break;

						case R.id.sbGain:
							eqband.gain_set(progress);  break;
					}

					String s = eqband.str();
					eqset.current_set(s);
					eEQ.setText(s);
				}
			}

			@Override
			public void onStartTrackingTouch(SeekBar seekBar) {
			}

			@Override
			public void onStopTrackingTouch(SeekBar seekBar) {
			}
		};
		sbFreq.setOnSeekBarChangeListener(sbcl);
		sbWidth.setOnSeekBarChangeListener(sbcl);
		sbGain.setOnSeekBarChangeListener(sbcl);

		return new AlertDialog.Builder(ctx)
				.setTitle("Equalizer")
				.setView(v)
				.setPositiveButton("OK", (dialog, which) -> {
					cb.f(eqset.commit());
				})
				.setNegativeButton("Cancel", null)
				.create();
	}

	private void switch_band(int band) {
		eqband = eqset.select(band);

		sbFreq.setProgress(eqband.freq_progress());
		sbFreq.setEnabled(!eqband.shelf());
		sbWidth.setProgress(eqband.width);
		sbWidth.setEnabled(!eqband.shelf());
		sbGain.setProgress(eqband.gain_progress());
		eEQ.setText(eqset.current());
	}

	private ArrayAdapter<String> spinner_adapter(Context ctx, String[] options) {
		ArrayAdapter<String> adapter = new ArrayAdapter<>(ctx, android.R.layout.simple_spinner_item, options);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		return adapter;
	}

	private void conf(Context ctx) {
		spBand.setAdapter(spinner_adapter(ctx, new String[]{
				"Low Shelf",
				"Band #2",
				"Band #3",
				"Band #4",
				"High Shelf",
		}));
		sbFreq.setMax(99);
		sbWidth.setMax(40); // 0..4.0
		sbGain.setMax((120 + 120) / 5); // -12.0..12.0 by 0.5
	}

	void show(Context ctx, String equ, Confirmed cb) {
		Dialog dlg = create(ctx, cb);
		eqset = new EQSet();
		eqband = new EQBand(0);
		conf(ctx);
		eqset.init(equ);
		switch_band(1);
		dlg.show();
	}
}
