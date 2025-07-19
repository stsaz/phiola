/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.ArrayAdapter;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

import com.github.stsaz.phiola.databinding.TagsBinding;

public class TagsActivity extends AppCompatActivity  {
	private static final String TAG = "phiola.TagsActivity";
	private Core core;
	private String[] meta;
	private boolean modified, clear, supported;
	private TagsBinding b;
	private int modified_bits;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = TagsBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		b.lvTags.setOnItemClickListener((parent, view, position, id) -> view_click(position));

		core = Core.getInstance();
		show();
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		getMenuInflater().inflate(R.menu.tags, menu);
		return true;
	}
	@Override
	public boolean onOptionsItemSelected(@NonNull MenuItem item) {
		switch (item.getItemId()) {

		case R.id.action_clear:
			tags_clear();  return true;

		case R.id.action_add:
			tag_add();  return true;

		case R.id.action_write:
			tags_write();  return true;
		}
		return super.onOptionsItemSelected(item);
	}

	@Override
	protected void onDestroy() {
		core.unref();
		super.onDestroy();
	}

	private void show() {
		meta = core.queue().active_meta();
		if (meta == null)
			meta = new String[0];
		redraw();
		if (meta.length == 0)
			return;

		String fn = meta[0+1];
		supported = fn.endsWith(".mp3")
			|| fn.endsWith(".ogg")
			|| fn.endsWith(".opus")
			|| fn.endsWith(".flac");
	}

	private void redraw() {
		ArrayList<String> tags = new ArrayList<>();
		String mod;
		for (int i = 0; i < meta.length; i+=2) {
			int k = i / 2 - Phiola.Meta.N_RESERVED;
			mod = "";
			if (k < 32 && (modified_bits & (1 << k)) != 0)
				mod = "(*)";
			tags.add(String.format("%s%s : %s", meta[i], mod, meta[i+1]));
		}
		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, R.layout.list_row, tags);
		b.lvTags.setAdapter(adapter);
	}

	private void view_click(int pos) {
		int i = pos * 2 + 1;
		if (pos < Phiola.Meta.N_RESERVED || i >= meta.length) {
			core.clipboard_text_set(this, meta[pos]);
			return;
		}

		if (!supported) {
			core.errlog(TAG, "Editing tags for this file format is not supported.");
			return;
		}

		core.gui().dlg_edit(this, "Edit Tag", meta[pos * 2], meta[i], "Apply", "Cancel"
			, (new_text) -> tag_edit_done(i, new_text));
	}

	private void tag_edit_done(int i, String new_text) {
		int k = i / 2 - Phiola.Meta.N_RESERVED;
		if (k >= 32) {
			core.errlog(TAG, "Too many tags");
			return;
		}
		meta[i] = new_text;
		modified_bits |= 1 << k;
		modified = true;
		redraw();
	}

	private void tags_clear() {
		meta = Arrays.copyOfRange(meta, 0, Phiola.Meta.N_RESERVED*2);
		clear = true;
		modified = true;
		modified_bits = 0;
		redraw();
	}

	private void tag_add() {
		core.gui().dlg_edit(this, "Add Tag", "Format: 'name=value'", "", "Add", "Cancel"
			, (s) -> tag_add_done(s));
	}

	private void tag_add_done(String tag) {
		int k = meta.length / 2 - Phiola.Meta.N_RESERVED;
		if (k >= 32) {
			core.errlog(TAG, "Too many tags");
			return;
		}

		ArrayList<String> m = new ArrayList<>();
		Collections.addAll(m, meta);
		int pos = tag.indexOf('=');
		m.add(tag.substring(0, pos));
		m.add(tag.substring(pos + 1));
		meta = m.toArray(new String[0]);

		modified = true;
		modified_bits |= 1 << k;
		redraw();
	}

	private void tags_write() {
		if (!modified || meta.length < Phiola.Meta.N_RESERVED*2) return;

		String fn = meta[0+1];

		// Prepare the list of modified tags
		ArrayList<String> tags = new ArrayList<>();
		int k = 0;
		for (int i = Phiola.Meta.N_RESERVED*2;  i + 1 < meta.length;  i += 2, k++) {
			if (k < 32 && (modified_bits & (1 << k)) != 0) {
				tags.add(meta[i]);
				tags.add(meta[i + 1]);
			}
		}

		int flags = 0;
		if (clear)
			flags |= Phiola.TE_CLEAR;

		int r = core.phiola.tagsEdit(fn, tags.toArray(new String[0]), flags);
		if (r != 0) {
			core.errlog(TAG, "Error: %s", core.errstr(r));
			return;
		}

		clear = false;
		modified = false;
		modified_bits = 0;
		core.gui().msg_show(this, "Written tags to file \"%s\"", fn);
	}
}
