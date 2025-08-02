/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.graphics.PorterDuff;
import android.os.Build;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;
import android.widget.SeekBar;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.appcompat.widget.SearchView;
import androidx.core.app.ActivityCompat;
import androidx.recyclerview.widget.LinearLayoutManager;

import java.io.File;

import com.github.stsaz.phiola.databinding.MainBinding;

public class MainActivity extends AppCompatActivity {
	private static final String TAG = "phiola.MainActivity";

	private Core core;
	private GUI gui;
	private Queue queue;
	private QueueNotify quenfy;
	private Track track;
	private PlaybackObserver trk_nfy;
	private TrackCtl trackctl;
	private long total_dur_msec;
	private int view_prev = GUI.V_EXPLORER;

	private MLib library;
	private Explorer explorer;
	private PlaylistAdapter pl_adapter;
	private PopupMenu mfile, mlist, mitem;
	private int item_menu_qi;

	private MainBinding b;

	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		if (0 != init_mods())
			return;
		b = MainBinding.inflate(getLayoutInflater());
		init_ui();
		init_system();

		b.list.setAdapter(pl_adapter);
		b.list.setItemAnimator(null);

		if (gui.cur_path.isEmpty())
			gui.cur_path = core.storage_path;

		if (core.setts.rec_path.isEmpty())
			core.setts.rec_path = core.storage_path + "/Recordings";

		// Add file to the playlist and start playback if executed from an external file manager app
		if (getIntent().getAction().equals(Intent.ACTION_VIEW)) {
			String fn = getIntent().getData().getPath();
			core.dbglog(TAG, "Intent.ACTION_VIEW: %s", fn);
			fn = Util.path_real(fn, core.storage_paths);
			if (fn != null)
				explorer_event(fn, Queue.ADD);
		}
	}

	protected void onStart() {
		super.onStart();
		core.dbglog(TAG, "onStart()");

		show_ui();

		// If already playing - get in sync
		track.observer_notify(trk_nfy);
	}

	protected void onResume() {
		super.onResume();
		if (core != null) {
			core.dbglog(TAG, "onResume()");
		}
	}

	protected void onStop() {
		if (core != null) {
			core.dbglog(TAG, "onStop()");
			queue.saveconf();
			list_leave();
			core.saveconf();
		}
		super.onStop();
	}

	public void onDestroy() {
		if (core != null) {
			core.dbglog(TAG, "onDestroy()");
			track.observer_rm(trk_nfy);
			trackctl.close();
			queue.nfy_rm(quenfy);
			core.close();
		}
		super.onDestroy();
	}

	public boolean onCreateOptionsMenu(Menu menu) {
		b.toolbar.inflateMenu(R.menu.menu);
		b.toolbar.setOnMenuItemClickListener(this::onOptionsItemSelected);
		return true;
	}

	public boolean onOptionsItemSelected(@NonNull MenuItem item) {
		switch (item.getItemId()) {
			case R.id.action_settings:
				startActivity(new Intent(this, SettingsActivity.class));
				return true;

			case R.id.action_play_auto_stop:
				play_auto_stop();
				return true;

			case R.id.action_file_menu_show:
				if (mfile == null) {
					mfile = new PopupMenu(this, findViewById(R.id.action_file_menu_show));
					mfile.getMenuInflater().inflate(R.menu.file, mfile.getMenu());
					mfile.setOnMenuItemClickListener(this::file_menu_click);
				}
				mfile.show();
				return true;

			case R.id.action_list_menu_show:
				if (mlist == null) {
					mlist = new PopupMenu(this, findViewById(R.id.action_list_menu_show));
					mlist.getMenuInflater().inflate(R.menu.list, mlist.getMenu());
					mlist.setOnMenuItemClickListener(this::list_menu_click);
				}
				mlist.show();
				return true;

			case R.id.action_about:
				startActivity(new Intent(this, AboutActivity.class));
				return true;
		}
		return super.onOptionsItemSelected(item);
	}

	private boolean file_menu_click(MenuItem item) {
		switch (item.getItemId()) {

		case R.id.action_file_tags_show:
			file_tags_show();  break;

		case R.id.action_file_convert:
			file_convert(); break;

		case R.id.action_file_showcur:
			explorer_file_current_show();  break;

		case R.id.action_file_del:
			file_del_cur();  break;

		case R.id.action_file_move:
			file_move_cur();  break;

		default:
			return false;
		}
		return true;
	}

	private boolean list_menu_click(MenuItem item) {
		switch (item.getItemId()) {
		case R.id.action_list_new:
			list_new();  break;

		case R.id.action_list_close:
			list_close();  break;

		case R.id.action_list_add:
			startActivity(new Intent(this, AddURLActivity.class));  break;

		case R.id.action_list_rm:
			list_rm();  break;

		case R.id.action_list_rm_non_existing:
			queue.current_remove_non_existing();  break;

		case R.id.action_list_clear:
			list_clear();  break;

		case R.id.action_list_save:
			list_save();  break;

		case R.id.action_list_rename:
			list_rename();  break;

		case R.id.action_list_showcur: {
			if (gui.view != GUI.V_PLAYLIST)
				plist_click();
			int pos = queue.active_pos();
			if (pos >= 0)
				b.list.scrollToPosition(pos);
			break;
		}

		case R.id.action_list_next_add_cur:
			list_next_add_cur();  break;

		case R.id.action_list_sort:
			list_sort_menu_show();  break;

		case R.id.action_move_left: {
			int i = queue.current_move_left();
			if (i >= 0) {
				gui.list_swap(i, i + 1);
				bplaylist_text(i);
			}
			break;
		}

		case R.id.action_list_convert:
			list_convert();  break;

		case R.id.action_list_fmove:
			list_files_move();  break;

		case R.id.action_list_read_meta:
			queue.current_read_meta();  break;

		default:
			return false;
		}
		return true;
	}

	private void list_sort_menu_show() {
		PopupMenu m = new PopupMenu(this, b.list);
		m.setOnMenuItemClickListener((item) -> {
				queue.current_sort(item.getItemId());
				return true;
			});
		m.getMenu().add(0, Phiola.QU_SORT_FILENAME, 0, getString(R.string.mlist_sort_filename));
		m.getMenu().add(0, Phiola.QU_SORT_FILESIZE, 0, getString(R.string.mlist_sort_filesize));
		m.getMenu().add(0, Phiola.QU_SORT_FILEDATE, 0, getString(R.string.mlist_sort_filedate));
		m.getMenu().add(0, Phiola.QU_SORT_RANDOM, 0, getString(R.string.mlist_shuffle));
		m.getMenu().add(0, Phiola.QU_SORT_TAG_ARTIST, 0, getString(R.string.mlist_sort_tag_artist));
		m.getMenu().add(0, Phiola.QU_SORT_TAG_DATE, 0, getString(R.string.mlist_sort_tag_date));
		m.show();
	}

	private boolean item_menu_click(MenuItem item) {
		switch (item.getItemId()) {

		case R.id.action_list_remove:
			queue.current_remove(item_menu_qi);  break;

		case R.id.action_file_explore:
			explorer_file_show(queue.visible_url(item_menu_qi));  break;

		case R.id.action_file_delete: {
			String fn = queue.visible_url(item_menu_qi);
			if (!fn.isEmpty())
				file_delete_ask(item_menu_qi, fn);
			break;
		}

		default:
			return false;
		}
		return true;
	}

	private void item_menu_show(int i) {
		item_menu_qi = i;
		if (mitem == null) {
			mitem = new PopupMenu(this, b.lname);
			mitem.getMenuInflater().inflate(R.menu.item, mitem.getMenu());
			mitem.setOnMenuItemClickListener(this::item_menu_click);
		}
		mitem.show();
	}

	private static final int
		REQUEST_PERM_READ_STORAGE = 1,
		REQUEST_PERM_RECORD = 2;

	/** Called by OS with the result of requestPermissions(). */
	public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
		super.onRequestPermissionsResult(requestCode, permissions, grantResults);
		if (grantResults.length != 0)
			core.dbglog(TAG, "onRequestPermissionsResult: %d: %d", requestCode, grantResults[0]);
	}

	static final int
		REQUEST_STORAGE_ACCESS = 1,
		REQUEST_CONVERT = 2;

	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		core.dbglog(TAG, "onActivityResult: requestCode:%d resultCode:%d", requestCode, resultCode);
		super.onActivityResult(requestCode, resultCode, data);

		if (requestCode == REQUEST_CONVERT && resultCode == RESULT_OK) {
			convert_started();
		}
	}

	/** Request system permissions */
	private void init_system() {
		String[] perms = new String[]{
				Manifest.permission.READ_EXTERNAL_STORAGE,
				Manifest.permission.WRITE_EXTERNAL_STORAGE,
		};
		for (String perm : perms) {
			if (ActivityCompat.checkSelfPermission(this, perm) != PackageManager.PERMISSION_GRANTED) {
				core.dbglog(TAG, "ActivityCompat.requestPermissions: %s", perm);
				ActivityCompat.requestPermissions(this, perms, REQUEST_PERM_READ_STORAGE);
				break;
			}
		}
	}

	private boolean user_ask_record() {
		String perm = Manifest.permission.RECORD_AUDIO;
		if (ActivityCompat.checkSelfPermission(this, perm) != PackageManager.PERMISSION_GRANTED) {
			core.dbglog(TAG, "ActivityCompat.requestPermissions: %s", perm);
			ActivityCompat.requestPermissions(this, new String[]{perm}, REQUEST_PERM_RECORD);
			return false;
		}
		return true;
	}

	/** Initialize core and modules */
	private int init_mods() {
		core = Core.init_once(getApplicationContext());
		if (core == null)
			return -1;
		core.dbglog(TAG, "init_mods()");

		gui = core.gui();
		queue = core.queue();
		quenfy = new QueueNotify() {
			public void on_change(int how, int pos) {
				list_on_change(how, pos);
			}
		};
		queue.nfy_add(quenfy);
		track = core.track;
		trk_nfy = new PlaybackObserver() {
			public int open(TrackHandle t) { return track_opening(t); }
			public void close(TrackHandle t) { track_closing(t); }
			public void closed(TrackHandle t) { track_closed(t); }
			public int process(TrackHandle t) { return track_update(t); }
		};
		track.observer_add(trk_nfy);
		trackctl = new TrackCtl(core, this);
		trackctl.connect();
		return 0;
	}

	/** Set UI objects and register event handlers */
	private void init_ui() {
		setContentView(b.getRoot());

		setSupportActionBar(b.toolbar);

		explorer = new Explorer(core, this);
		library = new MLib(core, this);

		b.lname.setOnClickListener((v) -> file_tags_show());

		b.brec.setOnClickListener((v) -> {
				if (core.setts.rec_longclick)
					rec_pause_toggle();
				else
					rec_start_stop();
			});
		b.brec.setOnLongClickListener((v) -> {
				if (core.setts.rec_longclick)
					rec_start_stop();
				return true;
			});

		b.bplay.setOnClickListener((v) -> play_pause_click());

		b.bprev.setOnClickListener((v) -> trackctl.prev());
		b.bprev.setOnLongClickListener((v) -> {
				seek_back();
				return true;
			});

		b.bnext.setOnClickListener((v) -> trackctl.next());
		b.bnext.setOnLongClickListener((v) -> {
				seek_fwd();
				return true;
			});

		b.bexplorer.setOnClickListener((v) -> explorer_click());

		b.bplaylist.setOnClickListener((v) -> plist_click());
		b.bplaylist.setOnLongClickListener((v) -> {
				playlist_menu_show();
				return true;
			});
		b.bplaylist.setChecked(true);
		bplaylist_text(queue.current_index());

		b.seekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			int val; // last value

			public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
				if (fromUser)
					val = progress;
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
				val = -1;
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
				if (val != -1)
					seek(val);
			}
		});

		b.tfilter.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
			public boolean onQueryTextSubmit(String query) {
				return true;
			}

			public boolean onQueryTextChange(String newText) {
				plist_filter(newText);
				return true;
			}
		});

		b.list.setLayoutManager(new LinearLayoutManager(this));
		pl_adapter = new PlaylistAdapter(this, new PlaylistViewHolder.Parent() {

				public int count() {
					if (gui.view == GUI.V_EXPLORER)
						return explorer.count();
					else if (gui.view == GUI.V_LIBRARY)
						return library.count();
					return queue.visible_items();
				}

				public String display_line(int pos) {
					if (gui.view == GUI.V_EXPLORER)
						return explorer.display_line(pos);
					else if (gui.view == GUI.V_LIBRARY)
						return library.display_line(pos);
					return queue.display_line(pos);
				}

				public void on_click(int i) {
					if (gui.view == GUI.V_EXPLORER)
						explorer.event(0, i);
					else if (gui.view == GUI.V_LIBRARY)
						library.on_click(i);
					else
						queue.visible_play(i);
				}

				public void on_longclick(int i) {
					if (gui.view == GUI.V_EXPLORER)
						explorer.event(1, i);
					else if (gui.view == GUI.V_LIBRARY)
						library.on_longclick(i);
					else
						item_menu_show(i);
				}
			});

		gui.cur_activity = this;
	}

	private void show_ui() {
		if (gui.view == GUI.V_PLAYLIST) {
			plist_show();
		} else {
			if (gui.view == GUI.V_LIBRARY)
				gui.view = GUI.V_EXPLORER;
			else if (gui.view == GUI.V_EXPLORER)
				gui.view = GUI.V_LIBRARY;
			explorer_click();
		}

		int mode = AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM;
		if (gui.theme == GUI.THM_DARK)
			mode = AppCompatDelegate.MODE_NIGHT_YES;
		AppCompatDelegate.setDefaultNightMode(mode);

		if (gui.record_hide)
			b.brec.setVisibility(View.INVISIBLE);

		if (gui.filter_hide)
			b.tfilter.setVisibility(View.INVISIBLE);

		int mask = GUI.MASK_PLAYBACK;
		int st = GUI.STATE_DEF;
		if (queue.auto_stop_active) {
			mask |= GUI.STATE_AUTO_STOP;
			st |= GUI.STATE_AUTO_STOP;
		}
		if (gui.state_test(GUI.STATE_RECORDING)) {
			rec_state_set(true);
		}
		state_f(mask, st, true);
	}

	private void rec_state_set(boolean active) {
		if (Build.VERSION.SDK_INT < 21)
			return;

		int res = R.color.control_button;
		if (active)
			res = R.color.recording;
		int color = getResources().getColor(res);
		b.brec.setImageTintMode(PorterDuff.Mode.SRC_IN);
		b.brec.setImageTintList(ColorStateList.valueOf(color));
	}

	private void rec_fin(int code, String filename) {
		rec_state_set(false);
		stopService(new Intent(this, RecSvc.class));
		if (code == 0) {
			if (core.setts.rec_list_add)
				queue.current_add(filename, 0);
			gui.msg_show(this, getString(R.string.main_rec_fin));
		}

		if (gui.state_test(GUI.STATE_RECORDING))
			rec_stop();
	}

	private void rec_stop() {
		state(GUI.STATE_RECORDING | GUI.STATE_REC_PAUSED, 0);
		String e = track.record_stop();
		if (e != null)
			core.errlog(TAG, String.format("%s: %s", getString(R.string.main_rec_err), e));
	}

	private void rec_start_stop() {
		if (gui.state_test(GUI.STATE_RECORDING))
			rec_stop();
		else
			rec_start();
	}

	private void rec_pause_toggle() {
		int r = track.record_pause_toggle();
		String s = "Long press to start recording";
		if (r >= 0) {
			s = "Paused Recording";
			int st = GUI.STATE_REC_PAUSED;
			if (r == 0) {
				s = "Resumed Recording";
				st = 0;
			}
			state(GUI.STATE_REC_PAUSED, st);
		}
		gui.msg_show(this, s);
	}

	private void play_pause_click() {
		if (track.state() == Track.STATE_PLAYING) {
			trackctl.pause();
		} else {
			trackctl.unpause();
		}
	}

	private void explorer_click() {
		b.bexplorer.setChecked(true);
		b.bplaylist.setChecked(false);
		list_leave();

		String name = null;
		if (gui.view == GUI.V_PLAYLIST) {
			gui.view = view_prev;
		} else if (gui.view == GUI.V_LIBRARY) {
			gui.view = GUI.V_EXPLORER;
			name = getString(R.string.bexplorer);
		} else {
			gui.view = GUI.V_LIBRARY;
			name = getString(R.string.blibrary);
		}

		if (name != null) {
			b.bexplorer.setText(name);
			b.bexplorer.setTextOn(name);
			b.bexplorer.setTextOff(name);
		}

		if (gui.view == GUI.V_EXPLORER) {
			explorer.fill();
			if (!gui.filter_hide)
				b.tfilter.setVisibility(View.INVISIBLE);
		} else {
			if (!gui.filter_hide) {
				b.tfilter.setVisibility(View.VISIBLE);
				b.tfilter.setQuery("", false);
			}
			library.fill();
		}

		list_update();
		if (gui.view == GUI.V_LIBRARY)
			list_scroll(gui.mlib_scroll_pos);
	}

	private void plist_click() {
		b.bplaylist.setChecked(true);
		if (gui.view == GUI.V_PLAYLIST) {
			list_switch();
			return;
		}

		list_leave();
		b.bexplorer.setChecked(false);
		view_prev = gui.view;
		gui.view = GUI.V_PLAYLIST;
		if (!gui.filter_hide) {
			b.tfilter.setQuery(gui.list_filter, false);
			b.tfilter.setVisibility(View.VISIBLE);
			queue.current_filter(gui.list_filter);
		}

		list_update();
		plist_show();
	}

	private String list_name(int i) {
		String s = gui.list_name(i);
		if (s.isEmpty())
			s = String.format(getString(R.string.main_playlist_n), i + 1);
		return s;
	}

	private void playlist_menu_show() {
		PopupMenu m = new PopupMenu(this, b.bplaylist);
		m.setOnMenuItemClickListener((item) -> {
				list_switch_i(item.getItemId());
				return true;
			});
		int n = queue.number();
		for (int i = 0;  i < n;  i++) {
			m.getMenu().add(0, i, 0, list_name(i));
		}
		m.show();
	}

	private void file_tags_show() { startActivity(new Intent(this, TagsActivity.class)); }

	/** Delete file and update view */
	private void file_del(int pos, String fn) {
		if (!core.setts.file_del) {
			String e = core.util.trash(core.setts.trash_dir, fn);
			if (!e.isEmpty()) {
				core.errlog(TAG, "Can't trash file %s: %s", fn, e);
				return;
			}
			gui.msg_show(this, "Moved file to Trash directory");
		} else {
			if (!core.file_delete(fn))
				return;
			gui.msg_show(this, "Deleted file");
		}

		plist_filter_clear();
		queue.active_remove(pos);
	}

	private void file_del_cur() {
		int pos = queue.active_pos();
		String fn = queue.active_track_url();
		if (fn != null)
			file_delete_ask(pos, fn);
	}

	private void file_delete_ask(int pos, String fn) {
		AlertDialog.Builder b = new AlertDialog.Builder(this);
		b.setIcon(android.R.drawable.ic_dialog_alert);
		b.setTitle("File Delete");
		String msg, btn;
		if (core.setts.file_del) {
			msg = String.format("Delete file from storage: %s ?", fn);
			btn = "Delete";
		} else {
			msg = String.format("Move file to Trash: %s ?", fn);
			btn = "Trash";
		}
		b.setMessage(msg);
		b.setPositiveButton(btn, (dialog, which) -> file_del(pos, fn));
		b.setNegativeButton("Cancel", null);
		b.show();
	}

	private void file_move_cur() {
		gui.dlg_question(this, "Move file"
			, String.format("Move the current file to %s ?", gui.cur_path)
			, "Move File", "Do nothing"
			, (dialog, which) -> { file_move_cur_confirmed(); }
			);
	}

	private void file_move_cur_confirmed() {
		String fn = queue.active_track_url();
		if (fn == null)
			return;

		String e = core.util.fileMove(fn, gui.cur_path);
		if (!e.isEmpty()) {
			core.errlog(TAG, "file move: %s", e);
			return;
		}

		gui.msg_show(this, "Moved file to %s", gui.cur_path);
	}

	private void plist_open_new(String fn) {
		list_new();

		int pos = queue.current_index();
		gui.list_name_set(pos, Util.path_split3(fn)[1]);
		bplaylist_text(pos);

		queue.current_add(fn, Queue.ADD);
		queue.current_play(0);
	}

	void library_event(String fn, int flags) {
		if (flags == 0) {
			plist_open_new(fn);
			return;
		}

		explorer_event(fn, Queue.ADD_RECURSE);
	}

	private boolean is_playlist(String ext) {
		return ext.equalsIgnoreCase("m3u")
			|| ext.equalsIgnoreCase("m3u8");
	}

	void explorer_event(String fn, int flags) {
		if (fn == null) {
			b.list.setAdapter(pl_adapter);
			return;
		}

		if (flags == Queue.ADD && is_playlist(Util.path_split3(fn)[2])) {
			plist_open_new(fn);
			return;
		}

		int n = queue.current_items();
		queue.current_add(fn, flags);
		gui.msg_show(this, "Added %d items to playlist", 1);
		if (flags == Queue.ADD)
			queue.current_play(n);
	}

	private void explorer_file_current_show() {
		String fn = track.cur_url();
		if (!fn.isEmpty())
			explorer_file_show(fn);
	}

	private void explorer_file_show(String fn) {
		gui.cur_path = new File(fn).getParent();
		if (gui.view != GUI.V_EXPLORER) {
			if (gui.view == GUI.V_PLAYLIST)
				gui.view = GUI.V_LIBRARY;
			explorer_click();
		} else {
			explorer.fill();
			list_update();
		}
		int pos = explorer.file_idx(fn);
		if (pos >= 0)
			b.list.scrollToPosition(pos);
	}

	private void list_on_change(int how, int pos) {
		if (how == QueueNotify.CONVERT_COMPLETE) {
			convert_complete();
			return;
		}

		if (gui.view == GUI.V_PLAYLIST)
			pl_adapter.on_change(how, pos);
	}

	private void list_update() {
		pl_adapter.on_change(0, -1);
	}

	private void plist_show() {
		list_scroll(gui.list_scroll_pos(queue.current_index()));
	}

	/** Called when we're leaving the current tab */
	void list_leave() {
		LinearLayoutManager llm = (LinearLayoutManager)b.list.getLayoutManager();
		int pos = llm.findFirstCompletelyVisibleItemPosition();

		if (gui.view == GUI.V_PLAYLIST) {
			queue.current_filter("");
			gui.list_filter = b.tfilter.getQuery().toString();
			gui.list_scroll_pos_set(queue.current_index(), pos);
		} else if (gui.view == GUI.V_LIBRARY) {
			gui.mlib_scroll_pos = pos;
		}
	}

	private void plist_filter_clear() {
		if (!b.tfilter.getQuery().toString().isEmpty()) {
			b.tfilter.setQuery("", false);
			plist_filter("");
		}
	}

	private void plist_filter(String filter) {
		core.dbglog(TAG, "list_filter: %s", filter);
		if (gui.view == GUI.V_PLAYLIST)
			queue.current_filter(filter);
		else if (gui.view == GUI.V_LIBRARY)
			library.filter(filter);
		list_update();
	}

	private void bplaylist_text(int qi) {
		String s;
		if (queue.conversion_list(qi))
			s = "Conversion";
		else
			s = list_name(qi);
		b.bplaylist.setText(s);
		b.bplaylist.setTextOn(s);
		b.bplaylist.setTextOff(s);
	}

	/** Toggle playback auto-stop timer */
	private void play_auto_stop() {
		String s;
		int value_min = queue.auto_stop_toggle();
		if (value_min > 0) {
			state(GUI.STATE_AUTO_STOP, GUI.STATE_AUTO_STOP);
			s = String.format(getString(R.string.mplay_auto_stop_msg), value_min);
		} else {
			state(GUI.STATE_AUTO_STOP, 0);
			s = "Disabled auto-stop timer";
		}
		gui.msg_show(this, s);
	}

	private void list_new() {
		int qi = queue.new_list();
		if (qi < 0)
			return;
		gui.lists_number(qi + 1);

		gui.msg_show(this, String.format(getString(R.string.mlist_created), qi+1));
		queue.switch_list(qi);
		if (gui.view != GUI.V_PLAYLIST)
			plist_click();
		else
			list_update();
		bplaylist_text(qi);
	}

	private void list_close() {
		if (gui.view != GUI.V_PLAYLIST) return;

		if (0 == queue.current_items()) {
			list_close_confirmed();
			return;
		}

		gui.dlg_question(this, "Close playlist"
			, "Close the current playlist?"
			, "Close list", "Do nothing"
			, (dialog, which) -> { list_close_confirmed(); }
			);
	}

	private void list_close_confirmed() {
		int qi = queue.current_index();
		if (queue.current_close() != 0) {
			core.errlog(TAG, "Please wait until the conversion is complete");
			return;
		}

		gui.list_closed(qi);
		gui.msg_show(this, getString(R.string.mlist_closed));
		list_update();
		bplaylist_text(queue.current_index());
	}

	private void list_clear() {
		if (0 == queue.current_items())
			return;

		gui.dlg_question(this, "Clear playlist"
			, "Remove all items from the current playlist?"
			, "Clear", "Do nothing"
			, (dialog, which) -> { queue.current_clear(); }
			);
	}

	/** Remove currently playing track from playlist */
	private void list_rm() {
		int pos = queue.active_pos();
		if (pos < 0)
			return;

		plist_filter_clear();
		queue.active_remove(pos);
		gui.msg_show(this, getString(R.string.mlist_trk_rm));
	}

	/** Show dialog for saving playlist file */
	private void list_save() {
		startActivity(new Intent(this, ListSaveActivity.class)
			.putExtra("name", gui.list_name(queue.current_index())));
	}

	private void list_rename() {
		int pos = queue.current_index();
		gui.dlg_edit(this, "Rename List", "Specify the name for this list:", list_name(pos), "Rename", "Cancel", (s) -> {
				gui.list_name_set(pos, s);
				bplaylist_text(pos);
			});
	}

	private void list_scroll(int n) {
		if (n != 0) {
			LinearLayoutManager llm = (LinearLayoutManager)b.list.getLayoutManager();
			llm.scrollToPositionWithOffset(n, 0);
		}
	}

	private void list_switched(int i) {
		queue.current_filter(gui.list_filter);
		list_update();
		bplaylist_text(i);
		list_scroll(gui.list_scroll_pos(i));
	}

	private void list_switch() {
		list_leave();
		int qi = queue.switch_list_next();
		list_switched(qi);
	}

	private void list_switch_i(int i) {
		list_leave();
		queue.switch_list(i);
		list_switched(i);
	}

	private void list_next_add_cur() {
		int qi = queue.next_list_add_cur();
		if (qi >= 0)
			gui.msg_show(this, String.format(getString(R.string.mlist_trk_added), qi+1));
	}

	private void list_files_move() {
		gui.dlg_question(this, "Move files"
			, String.format("Move all files in the current list to %s ?", gui.cur_path)
			, "Move All Files", "Do nothing"
			, (dialog, which) -> { list_files_move_confirmed(); }
			);
	}

	private void list_files_move_confirmed() {
		int n = queue.move_all(gui.cur_path);
		String s = String.format("Moved %d files", n);
		if (n < 0)
			s = "Some files were NOT moved";
		gui.msg_show(this, s);
	}

	static String q_error(int e) {
		switch (e) {
		case Queue.E_EXIST:
			return "Please close the existing Conversion list";
		case Queue.E_NOENT:
			return "Please navigate to a list you want to convert";
		}
		return "";
	}

	private void list_convert() {
		long qi_old = queue.current_id();
		int trk_pos = queue.active_pos();
		int qi = queue.convert_add(Queue.CONV_CUR_LIST);
		if (qi < 0) {

			if (qi == Queue.E_BUSY) {
				convert_busy();
				return;
			}

			core.errlog(TAG, q_error(qi));
			return;
		}

		gui.lists_number(qi + 1);

		if (gui.view != GUI.V_PLAYLIST)
			plist_click();
		else
			list_update();

		bplaylist_text(qi);
		startActivityForResult(new Intent(this, ConvertActivity.class)
				.putExtra("current_list_id", qi_old)
				.putExtra("active_track_pos", trk_pos)
			, REQUEST_CONVERT);
	}

	private void file_convert() {
		long qi_old = queue.current_id();
		int trk_pos = queue.active_pos();
		int qi = queue.convert_add(Queue.CONV_CUR_FILE);
		if (qi < 0) {

			if (qi == Queue.E_BUSY) {
				convert_busy();
				return;
			}

			String e = q_error(qi);
			if (qi == Queue.E_NOENT)
				e = "Please start playback of the file you want to convert";
			core.errlog(TAG, e);
			return;
		}

		gui.lists_number(qi + 1);

		if (gui.view != GUI.V_PLAYLIST)
			plist_click();
		else
			list_update();

		bplaylist_text(qi);
		startActivityForResult(new Intent(this, ConvertActivity.class)
				.putExtra("current_list_id", qi_old)
				.putExtra("active_track_pos", trk_pos)
				.putExtra("iname", track.cur_url())
				.putExtra("length", total_dur_msec)
			, REQUEST_CONVERT);
	}

	private void convert_started() {
		state(GUI.STATE_CONVERTING, GUI.STATE_CONVERTING);
		startService(new Intent(this, RecSvc.class));
	}

	private void convert_complete() {
		stopService(new Intent(this, RecSvc.class));
		state(GUI.STATE_CONVERTING, 0);
	}

	private void convert_busy() {
		gui.dlg_question(this, "Conversion in progress"
			, "Conversion is already in progress.\nDo you want to interrupt it?"
			, "Interrupt", "Do nothing"
			, (dialog, which) -> { convert_cancel(); }
			);
	}

	private void convert_cancel() {
		queue.convert_cancel();
		gui.msg_show(this, "Conversion has been interrupted");
	}

	/** Start recording */
	private void rec_start() {
		if (!user_ask_record())
			return;

		TrackHandle trec = track.rec_start((code, filename) -> {
				core.tq.post(() -> {
						rec_fin(code, filename);
					});
			});
		if (trec == null)
			return;
		rec_state_set(true);
		gui.msg_show(this, getString(R.string.main_rec_started));
		startService(new Intent(this, RecSvc.class));
		state(GUI.STATE_RECORDING, GUI.STATE_RECORDING);
	}

	/** UI event from seek bar */
	private void seek(int percent) {
		trackctl.seek((total_dur_msec / 1000) * percent / 100 * 1000);
	}

	private void seek_back() {
		int percent = b.seekbar.getProgress() - core.setts.play_seek_back_percent;
		if (percent < 0)
			percent = 0;
		seek(percent);
	}

	private void seek_fwd() {
		int percent = b.seekbar.getProgress() + core.setts.play_seek_fwd_percent;
		if (percent >= 100)
			percent = 99;
		seek(percent);
	}

	// [Playing]
	// [PLA,STP,REC|RPA,CON]
	private String state_flags(int st) {
		if ((st & (GUI.STATE_PLAYING | GUI.STATE_RECORDING | GUI.STATE_CONVERTING)) == 0)
			return "";

		String s = "[";

		if ((st & GUI.STATE_PLAYING) != 0) {
			if (st == GUI.STATE_PLAYING)
				s += getString(R.string.main_st_playing);
			else
				s += "PLA";

			if ((st & GUI.STATE_AUTO_STOP) != 0)
				s += ",STP";
		}

		if ((st & GUI.STATE_RECORDING) != 0) {
			if ((st & GUI.STATE_PLAYING) != 0)
				s += ",";
			if ((st & GUI.STATE_REC_PAUSED) != 0)
				s += "RPA";
			else
				s += "REC";
		}

		if ((st & GUI.STATE_CONVERTING) != 0) {
			if ((st & (GUI.STATE_PLAYING | GUI.STATE_RECORDING)) != 0)
				s += ",";
			s += "CON";
		}

		s += "]";
		return s;
	}

	private void state(int mask, int val) { state_f(mask, val, false); }
	private void state_f(int mask, int val, boolean force) {
		int old = gui.state_update(mask, val);
		int st = (old & ~mask) | val;
		if (!force && st == old)
			return;

		String title = "φphiola";
		getSupportActionBar().setTitle(String.format("%s %s", title, state_flags(st)));

		if ((st & GUI.MASK_PLAYBACK) != (old & GUI.MASK_PLAYBACK)) {
			int play_icon = R.drawable.ic_play;
			if ((st & GUI.STATE_PLAYING) != 0)
				play_icon = R.drawable.ic_pause;
			b.bplay.setImageResource(play_icon);
		}
	}

	/** Called by Track when a new track is initialized */
	private int track_opening(TrackHandle t) {
		String title = t.name;
		if (!t.pmeta.date.isEmpty())
			title = String.format("%s [%s]", title, t.pmeta.date);
		if (gui.ainfo_in_title && !t.pmeta.info.isEmpty())
			title = String.format("%s [%s]", title, t.pmeta.info);
		b.lname.setText(title);

		b.seekbar.setProgress(0);
		if (t.state == Track.STATE_PAUSED) {
			state(GUI.MASK_PLAYBACK, GUI.STATE_PAUSED);
		} else {
			state(GUI.MASK_PLAYBACK, GUI.STATE_PLAYING);
		}
		return 0;
	}

	/** Called by Track after a track is finished */
	private void track_closing(TrackHandle t) {
		b.lname.setText("");
		b.lpos.setText("");
		b.seekbar.setProgress(0);
	}

	private void track_closed(TrackHandle t) {
		int st = GUI.STATE_DEF;
		if (queue.auto_stop_active)
			st |= GUI.STATE_AUTO_STOP;
		state(GUI.MASK_PLAYBACK | GUI.STATE_AUTO_STOP, st);
	}

	/** Called by Track during playback */
	private int track_update(TrackHandle t) {
		core.dbglog(TAG, "track_update: state:%d pos:%d", t.state, t.pos_msec);
		switch (t.state) {
		case Track.STATE_PAUSED:
			state(GUI.MASK_PLAYBACK, GUI.STATE_PAUSED);
			break;

		case Track.STATE_PLAYING:
			state(GUI.MASK_PLAYBACK, GUI.STATE_PLAYING);
			break;
		}

		String s;
		int progress = 0;
		long pos = t.pos_msec / 1000;
		long pos_min = pos / 60;
		int pos_sec = (int)(pos % 60);
		total_dur_msec = t.pmeta.length_msec;

		if (t.pmeta.length_msec == 0) {
			s = String.format("%d:%02d / --"
				, pos_min, pos_sec);

		} else {
			long dur = t.pmeta.length_msec / 1000;
			if (dur != 0)
				progress = (int)(pos * 100 / dur);

			s = String.format("%d:%02d / %d:%02d"
				, pos_min, pos_sec, dur / 60, dur % 60);
		}

		b.seekbar.setProgress(progress);
		b.lpos.setText(s);
		return 0;
	}
}
