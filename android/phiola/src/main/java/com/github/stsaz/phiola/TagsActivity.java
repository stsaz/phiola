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
	private int list_pos;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		b = TagsBinding.inflate(getLayoutInflater());
		setContentView(b.getRoot());

		b.lvTags.setOnItemClickListener((parent, view, position, id) -> view_click(position));

		core = Core.getInstance();
		this.list_pos = getIntent().getIntExtra("pos", -1);
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
			tags_clear();  break;

		case R.id.action_add_artist:
			tag_add_common_ask("artist", 0);  break;

		case R.id.action_add_title:
			tag_add_common_ask("title", 1);  break;

		case R.id.action_add:
			tag_add_ask();  break;

		case R.id.action_write:
			tags_write();  break;

		default:
			return super.onOptionsItemSelected(item);
		}

		return true;
	}

	@Override
	protected void onDestroy() {
		core.unref();
		super.onDestroy();
	}

	private void show() {
		if (list_pos >= 0)
			meta = core.queue().visible_meta(list_pos);
		else
			meta = core.queue().active_meta();
		if (meta == null)
			meta = new String[0];

		redraw();
		if (meta.length == 0)
			return;

		String ext = Util.path_split3(meta[0+1])[2];
		this.supported = ext.equalsIgnoreCase("mp3")
			|| ext.equalsIgnoreCase("ogg")
			|| ext.equalsIgnoreCase("opus")
			|| ext.equalsIgnoreCase("flac");
	}

	private boolean tag_modified(int k) { return (k < 32 && (modified_bits & (1 << k)) != 0); }

	private void redraw() {
		ArrayList<String> tags = new ArrayList<>();
		String mod;
		for (int i = 0; i < meta.length; i+=2) {
			int k = i / 2 - Phiola.Meta.N_RESERVED;
			mod = "";
			if (tag_modified(k))
				mod = "(*)";
			tags.add(String.format("%s%s : %s", meta[i], mod, meta[i+1]));
		}
		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, R.layout.list_row, tags);
		b.lvTags.setAdapter(adapter);
	}

	private void view_click(int pos) {
		int i = pos * 2 + 1;
		if (pos < Phiola.Meta.N_RESERVED || i >= meta.length) {
			core.clipboard_text_set(this, meta[i]);
			return;
		}

		if (!edit_supported())
			return;

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

	private boolean edit_supported() {
		if (!this.supported) {
			core.errlog(TAG, getString(R.string.tag_edit_n_a));
			return false;
		}
		return true;
	}

	private void tag_add_ask() {
		if (!edit_supported())
			return;

		core.gui().dlg_edit(this, "Add Tag", "Format: 'name=value'", "", "Add", "Cancel"
			, (s) -> tag_add_split(s));
	}

	private void tag_add_common_ask(String name, int pos) {
		if (!edit_supported())
			return;

		String value = "",  fn = Util.path_split3(meta[0+1])[1];
		if (pos == 1)
			value = fn;
		int dash = fn.lastIndexOf(" - ");
		if (dash > 0) {
			if (pos == 0)
				value = fn.substring(0, dash);
			else
				value = fn.substring(dash + 3);
		}

		core.gui().dlg_edit(this, "Add Tag", name, value, "Add", "Cancel"
			, (s) -> tag_add(name, s));
	}

	private void tag_add_split(String tag) {
		int pos = tag.indexOf('=');
		if (pos <= 0)
			return;
		tag_add(tag.substring(0, pos), tag.substring(pos + 1));
	}

	private void tag_add(String name, String value) {
		int k = meta.length / 2 - Phiola.Meta.N_RESERVED;
		if (k >= 32) {
			core.errlog(TAG, "ERROR: Too many tags");
			return;
		}

		for (int i = Phiola.Meta.N_RESERVED*2;  i + 1 < meta.length;  i += 2) {
			if (name.equalsIgnoreCase(meta[i])) {
				core.errlog(TAG, "ERROR: Tag already exists");
				return;
			}
		}

		ArrayList<String> m = new ArrayList<>();
		Collections.addAll(m, meta);
		m.add(name);
		m.add(value);
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
			if (tag_modified(k)) {
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
