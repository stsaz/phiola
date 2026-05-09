/** phiola/Android
2026, Simon Zolin */

package com.github.stsaz.phiola;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.graphics.PorterDuff;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.SeekBar;

public class DlgColor {
	private SeekBar sbRed, sbGreen, sbBlue;
	private EditText eColor;
	private int color;

	interface Confirmed {
		void f(int color);
	}

	private Dialog create(Context ctx, Confirmed cb) {
		View v = LayoutInflater.from(ctx).inflate(R.layout.color, null);
		sbRed = v.findViewById(R.id.sbRed);
		sbGreen = v.findViewById(R.id.sbGreen);
		sbBlue = v.findViewById(R.id.sbBlue);
		eColor = v.findViewById(R.id.eColor);

		SeekBar.OnSeekBarChangeListener sbcl = new SeekBar.OnSeekBarChangeListener() {
			@Override
			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				if (fromUser) {
					switch (seekBar.getId()) {
						case R.id.sbRed:
							apply(0x00ffff, (progress * 3) << 16);  break;

						case R.id.sbGreen:
							apply(0xff00ff, (progress * 3) << 8);  break;

						case R.id.sbBlue:
							apply(0xffff00, progress * 3);  break;
					}
				}
			}

			@Override
			public void onStartTrackingTouch(SeekBar seekBar) {
			}

			@Override
			public void onStopTrackingTouch(SeekBar seekBar) {
			}
		};
		sbRed.setOnSeekBarChangeListener(sbcl);
		sbGreen.setOnSeekBarChangeListener(sbcl);
		sbBlue.setOnSeekBarChangeListener(sbcl);

		return new AlertDialog.Builder(ctx)
				.setTitle("Choose color")
				.setView(v)
				.setPositiveButton("OK", (dialog, which) -> {
					cb.f(Util.color_from_str(eColor.getText().toString(), -1));
				})
				.setNegativeButton("Cancel", null)
				.create();
	}

	private static void seekbar_color(SeekBar sb, int color) {
		sb.getProgressDrawable().setColorFilter(0xff000000 | color, PorterDuff.Mode.SRC_IN);
		sb.getThumb().setColorFilter(0xff000000 | color, PorterDuff.Mode.SRC_IN);
	}

	private void apply(int mask, int value) {
		if (color < 0)
			color = 0; // default -> black
		color = (color & mask) | value;
		changed(color);
	}

	private void changed(int val) {
		eColor.setText(Util.color_str(val));
		eColor.setBackgroundColor(0xff000000 | val);
	}

	private void conf() {
		sbRed.setMax(256 / 3);
		seekbar_color(sbRed, 0xff4136);

		sbGreen.setMax(256 / 3);
		seekbar_color(sbGreen, 0x2ecc40);

		sbBlue.setMax(256 / 3);
		seekbar_color(sbBlue, 0x0074d9);
	}

	private void load() {
		sbRed.setProgress(((color & 0xff0000) >> 16) / 3);
		sbGreen.setProgress(((color & 0x00ff00) >> 8) / 3);
		sbBlue.setProgress((color & 0x0000ff) / 3);
		changed(color);
	}

	void show(Context ctx, int color, Confirmed cb) {
		Dialog dlg = create(ctx, cb);
		conf();

		this.color = color;
		if (color >= 0)
			load();

		dlg.show();
	}
}
