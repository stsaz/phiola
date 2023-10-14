/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;

import com.github.stsaz.phiola.databinding.ConvertBinding;

public class ConvertActivity extends AppCompatActivity {
	private static final String TAG = "phiola.ConvertActivity";
	Core core;
	private int qu_cur_pos;
	private ConvertBinding b;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = ConvertBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());
		b.bfromSetCur.setOnClickListener((v) -> pos_set_cur(true));
		b.untilSetCur.setOnClickListener((v) -> pos_set_cur(false));
		b.bstart.setOnClickListener((v) -> convert());

		core = Core.getInstance();
		load();

		qu_cur_pos = core.queue().cur();
		String iname = getIntent().getStringExtra("iname");
		b.tiname.setText(iname);

		int sl = iname.lastIndexOf('/');
		if (sl < 0)
			sl = 0;
		else
			sl++;
		b.todir.setText(iname.substring(0, sl));

		int pos = iname.lastIndexOf('.');
		if (pos < 0)
			pos = 0;
		b.toname.setText(iname.substring(sl, pos));
	}

	private int conv_extension(String s) {
		for (int i = 0;  i < core.setts.conv_extensions.length;  i++) {
			if (core.setts.conv_extensions[i].equals(s))
				return i;
		}
		return -1;
	}

	private void load() {
		ArrayAdapter<String> adapter = new ArrayAdapter<>(
			this,
			android.R.layout.simple_spinner_item,
			core.setts.conv_extensions
		);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		b.oext.setAdapter(adapter);
		b.oext.setSelection(conv_extension(core.setts.conv_outext));

		b.taacQ.setText(core.int_to_str(core.setts.conv_aac_quality));
		b.topusQ.setText(core.int_to_str(core.setts.conv_opus_quality));
		b.bcopy.setChecked(core.setts.conv_copy);
	}

	private void save() {
		core.setts.conv_outext = core.setts.conv_extensions[b.oext.getSelectedItemPosition()];
		core.setts.conv_copy = b.bcopy.isChecked();
		int v = core.str_to_uint(b.taacQ.getText().toString(), 0);
		if (v != 0)
			core.setts.conv_aac_quality = v;

		v = core.str_to_uint(b.topusQ.getText().toString(), 0);
		if (v != 0)
			core.setts.conv_opus_quality = v;
	}

	@Override
	protected void onDestroy() {
		save();
		core.unref();
		super.onDestroy();
	}

	/** Set 'from/until' position equal to the current playing position */
	private void pos_set_cur(boolean from) {
		int sec = core.track().curpos_msec() / 1000;
		String s = String.format("%d:%02d", sec/60, sec%60);
		if (from)
			b.tfrom.setText(s);
		else
			b.tuntil.setText(s);
	}

	String iname, oname;
	private void convert() {
		b.bstart.setEnabled(false);
		b.lresult.setText("Working...");

		Phiola.ConvertParams p = new Phiola.ConvertParams();
		p.from_msec = b.tfrom.getText().toString();
		p.to_msec = b.tuntil.getText().toString();
		p.copy = b.bcopy.isChecked();
		p.sample_rate = core.str_to_uint(b.tsampleRate.getText().toString(), 0);
		p.aac_quality = core.str_to_uint(b.taacQ.getText().toString(), 0);
		p.opus_quality = core.str_to_uint(b.topusQ.getText().toString(), 0);

		if (b.bpreserveDate.isChecked())
			p.flags |= Phiola.ConvertParams.F_DATE_PRESERVE;
		if (false)
			p.flags |= Phiola.ConvertParams.F_OVERWRITE;

		iname = b.tiname.getText().toString();
		oname = String.format("%s/%s.%s"
			, b.todir.getText().toString()
			, b.toname.getText().toString()
			, core.setts.conv_extensions[b.oext.getSelectedItemPosition()]);
		core.phiola.convert(iname, oname, p,
			(result) -> {
				Handler mloop = new Handler(Looper.getMainLooper());
				mloop.post(() -> {
					convert_done(result);
				});
			}
		);
	}

	private void convert_done(String result) {
		boolean ok = result.isEmpty();
		if (ok)
			b.lresult.setText("DONE!");
		else
			b.lresult.setText(result);

		if (ok && b.bplAdd.isChecked()) {
			core.queue().add(oname);
		}

		if (ok && b.btrashOrig.isChecked() && !iname.equals(oname)) {
			trash_input_file(iname);
		}
		b.bstart.setEnabled(true);
	}

	private void trash_input_file(String iname) {
		String e = core.phiola.trash(core.setts.trash_dir, iname);
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
