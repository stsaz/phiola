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
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;

import com.github.stsaz.phiola.databinding.MainBinding;

public class MainActivity extends AppCompatActivity {
	private static final String TAG = "phiola.MainActivity";
	private static final int REQUEST_PERM_READ_STORAGE = 1;
	private static final int REQUEST_PERM_RECORD = 2;
	static final int REQUEST_STORAGE_ACCESS = 1;

	private Core core;
	private GUI gui;
	private Queue queue;
	private QueueNotify quenfy;
	private Track track;
	private Filter trk_nfy;
	private TrackCtl trackctl;
	private long total_dur_msec;

	private boolean view_explorer;
	private Explorer explorer;
	private PlaylistAdapter pl_adapter;
	private PopupMenu mfile, mlist;

	private MainBinding b;

	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		if (0 != init_mods())
			return;
		b = MainBinding.inflate(getLayoutInflater());
		init_ui();
		init_system();

		b.list.setAdapter(pl_adapter);
		plist_show();

		if (gui.cur_path.isEmpty())
			gui.cur_path = core.storage_path;

		if (core.setts.rec_path.isEmpty())
			core.setts.rec_path = core.storage_path + "/Recordings";
	}

	protected void onStart() {
		super.onStart();
		core.dbglog(TAG, "onStart()");

		show_ui();

		// If already playing - get in sync
		track.filter_notify(trk_nfy);
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
			if (!view_explorer)
				pl_leave();
			core.saveconf();
		}
		super.onStop();
	}

	public void onDestroy() {
		if (core != null) {
			core.dbglog(TAG, "onDestroy()");
			track.filter_rm(trk_nfy);
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
			startActivity(new Intent(this, ConvertActivity.class)
					.putExtra("iname", track.cur_url())
					.putExtra("length", total_dur_msec));
			break;

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
			queue.rm_non_existing();  break;

		case R.id.action_list_clear:
			queue.clear();  break;

		case R.id.action_list_save:
			list_save();  break;

		case R.id.action_list_showcur: {
			if (view_explorer)
				plist_click();
			int pos = queue.cur();
			if (pos >= 0)
				b.list.scrollToPosition(pos);
			break;
		}

		case R.id.action_list_next_add_cur:
			list_next_add_cur();  break;

		case R.id.action_list_sort:
			queue.sort(Phiola.QU_SORT_FILENAME);  break;

		case R.id.action_list_shuffle:
			queue.sort(Phiola.QU_SORT_RANDOM);  break;

		default:
			return false;
		}
		return true;
	}

	/** Called by OS with the result of requestPermissions(). */
	public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
		super.onRequestPermissionsResult(requestCode, permissions, grantResults);
		if (grantResults.length != 0)
			core.dbglog(TAG, "onRequestPermissionsResult: %d: %d", requestCode, grantResults[0]);
	}

	public void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);

		if (resultCode != RESULT_OK) {
			if (resultCode != RESULT_CANCELED)
				core.errlog(TAG, "onActivityResult: requestCode:%d resultCode:%d", requestCode, resultCode);
			return;
		}

		/*switch (requestCode) {
			case REQUEST_STORAGE_ACCESS:
				if (Environment.isExternalStorageManager()) {
				}
				break;
		}*/
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
				if (view_explorer) return;

				pl_adapter.on_change(how, pos);
			}
		};
		queue.nfy_add(quenfy);
		track = core.track;
		trk_nfy = new Filter() {
			public int open(TrackHandle t) { return track_opening(t); }
			public void close(TrackHandle t) { track_closing(t); }
			public void closed(TrackHandle t) { track_closed(t); }
			public int process(TrackHandle t) { return track_update(t); }
		};
		track.filter_add(trk_nfy);
		trackctl = new TrackCtl(core, this);
		trackctl.connect();
		return 0;
	}

	/** Set UI objects and register event handlers */
	private void init_ui() {
		setContentView(b.getRoot());

		setSupportActionBar(b.toolbar);

		explorer = new Explorer(core, this);

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

		b.bnext.setOnClickListener((v) -> trackctl.next());

		b.bprev.setOnClickListener((v) -> trackctl.prev());

		b.bexplorer.setOnClickListener((v) -> explorer_click());

		b.bplaylist.setOnClickListener((v) -> plist_click());
		b.bplaylist.setChecked(true);
		bplaylist_text(queue.current_list_index());

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
		pl_adapter = new PlaylistAdapter(this, explorer);

		gui.cur_activity = this;
	}

	private void show_ui() {
		int mode = AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM;
		if (core.gui().theme == GUI.THM_DARK)
			mode = AppCompatDelegate.MODE_NIGHT_YES;
		AppCompatDelegate.setDefaultNightMode(mode);

		if (gui.record_hide)
			b.brec.setVisibility(View.INVISIBLE);

		if (gui.filter_hide)
			b.tfilter.setVisibility(View.INVISIBLE);

		int mask = GUI.MASK_PLAYBACK;
		int st = GUI.STATE_DEF;
		if (queue.auto_stop.armed()) {
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

	private void rec_stop() {
		String e = track.record_stop();
		rec_state_set(false);
		stopService(new Intent(this, RecSvc.class));
		state(GUI.STATE_RECORDING, 0);
		if (e != null)
			core.errlog(TAG, String.format("%s: %s", getString(R.string.main_rec_err), e));
		else
			gui.msg_show(this, getString(R.string.main_rec_fin));
	}

	private void rec_start_stop() {
		if (gui.state_test(GUI.STATE_RECORDING))
			rec_stop();
		else
			rec_start();
	}

	private void rec_pause_toggle() {
		int r = track.record_pause_toggle();
		String s = null;
		if (r < 0) {
			s = "Long press to start recording";
		} else {
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

	void explorer_click() {
		b.bexplorer.setChecked(true);
		if (view_explorer) return;

		pl_leave();
		view_explorer = true;
		b.bplaylist.setChecked(false);
		b.tfilter.setVisibility(View.INVISIBLE);

		explorer.fill();
		pl_adapter.view_explorer = true;
		list_update();
	}

	void plist_click() {
		b.bplaylist.setChecked(true);
		if (!view_explorer) {
			list_switch();
			return;
		}

		view_explorer = false;
		b.bexplorer.setChecked(false);
		if (!gui.filter_hide)
			b.tfilter.setVisibility(View.VISIBLE);

		pl_adapter.view_explorer = false;
		list_update();
		plist_show();
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
		queue.remove(pos);
	}

	/** Ask confirmation before deleting the currently playing file from storage */
	private void file_del_cur() {
		int pos = queue.cur();
		if (pos < 0)
			return;
		String fn = queue.get(pos);

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
		if (core.setts.quick_move_dir.isEmpty()) {
			core.errlog(TAG, "Please set move-directory in Settings");
			return;
		}

		int pos = queue.cur();
		if (pos < 0)
			return;
		String fn = queue.get(pos);

		String e = core.util.fileMove(fn, core.setts.quick_move_dir);
		if (!e.isEmpty()) {
			core.errlog(TAG, "file move: %s", e);
			return;
		}

		gui.msg_show(this, "Moved file to %s", core.setts.quick_move_dir);
	}

	void explorer_event(String fn, int flags) {
		if (fn == null) {
			b.list.setAdapter(pl_adapter);
			return;
		}

		int n = queue.count();
		String[] ents = new String[1];
		ents[0] = fn;
		queue.addmany(ents, flags);
		core.dbglog(TAG, "added %d items", ents.length);
		gui.msg_show(this, "Added %d items to playlist", ents.length);
		if (flags == Queue.ADD)
			queue.play(n);
	}

	private void explorer_file_current_show() {
		String fn = track.cur_url();
		if (fn.isEmpty())
			return;
		gui.cur_path = new File(fn).getParent();
		if (!view_explorer) {
			explorer_click();
		} else {
			explorer.fill();
			pl_adapter.view_explorer = true;
			list_update();
		}
		int pos = explorer.file_idx(fn);
		if (pos >= 0)
			b.list.scrollToPosition(pos);
	}

	private void list_update() {
		pl_adapter.on_change(0, -1);
	}

	private void plist_show() {
		b.list.scrollToPosition(gui.list_pos);
	}

	/** Called when we're leaving the playlist tab */
	void pl_leave() {
		queue.filter("");
		LinearLayoutManager llm = (LinearLayoutManager)b.list.getLayoutManager();
		gui.list_pos = llm.findLastCompletelyVisibleItemPosition();
	}

	private void plist_filter(String filter) {
		core.dbglog(TAG, "list_filter: %s", filter);
		queue.filter(filter);
		list_update();
	}

	private void bplaylist_text(int qi) {
		String s = String.format(getString(R.string.main_playlist_n), qi + 1);
		b.bplaylist.setText(s);
		b.bplaylist.setTextOn(s);
		b.bplaylist.setTextOff(s);
	}

	/** Toggle playback auto-stop timer */
	private void play_auto_stop() {
		String s;
		int value_min = queue.auto_stop.toggle();
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

		gui.msg_show(this, String.format(getString(R.string.mlist_created), qi+1));
		queue.switch_list(qi);
		if (view_explorer)
			plist_click();
		else
			list_update();
		bplaylist_text(qi);
	}

	private void list_close() {
		if (view_explorer) return;

		queue.close_current_list();
		gui.msg_show(this, getString(R.string.mlist_closed));
		list_update();
		bplaylist_text(queue.current_list_index());
	}

	/** Remove currently playing track from playlist */
	private void list_rm() {
		int pos = queue.cur();
		if (pos < 0)
			return;

		queue.remove(pos);
		gui.msg_show(this, getString(R.string.mlist_trk_rm));
	}

	/** Show dialog for saving playlist file */
	private void list_save() {
		startActivity(new Intent(this, ListSaveActivity.class));
	}

	private void list_switch() {
		int qi = queue.next_list_select();
		if (view_explorer)
			plist_click();
		else
			list_update();
		bplaylist_text(qi);
	}

	private void list_next_add_cur() {
		int qi = queue.next_list_add_cur();
		if (qi >= 0)
			gui.msg_show(this, String.format(getString(R.string.mlist_trk_added), qi+1));
	}

	/** Start recording */
	private void rec_start() {
		if (!user_ask_record())
			return;

		core.dir_make(core.setts.rec_path);
		Date d = new Date();
		Calendar cal = new GregorianCalendar();
		cal.setTime(d);
		int dt[] = {
				cal.get(Calendar.YEAR),
				cal.get(Calendar.MONTH) + 1,
				cal.get(Calendar.DAY_OF_MONTH),
				cal.get(Calendar.HOUR_OF_DAY),
				cal.get(Calendar.MINUTE),
				cal.get(Calendar.SECOND),
		};
		String fname = String.format("%s/rec_%04d%02d%02d_%02d%02d%02d.%s"
				, core.setts.rec_path, dt[0], dt[1], dt[2], dt[3], dt[4], dt[5]
				, core.setts.rec_fmt);
		TrackHandle trec = track.rec_start(fname, () -> {
				core.tq.post(this::rec_stop);
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
		trackctl.seek(total_dur_msec * percent / 100);
	}

	// [Playing]
	// [PLA,STP,REC]
	private String state_flags(int st) {
		if (core.gui().state_hide) return "";

		String s = "";
		if ((st & (GUI.STATE_PLAYING | GUI.STATE_RECORDING)) != 0) {
			s = "[";

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

			s += "]";
		}
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
		if (!t.date.isEmpty())
			title = String.format("%s [%s]", title, t.date);
		if (core.gui().ainfo_in_title && !t.info.isEmpty())
			title = String.format("%s [%s]", title, t.info);
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
		if (queue.auto_stop.armed())
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

		long pos = t.pos_msec / 1000;
		total_dur_msec = t.time_total_msec;
		long dur = t.time_total_msec / 1000;

		int progress = 0;
		if (dur != 0)
			progress = (int)(pos * 100 / dur);
		b.seekbar.setProgress(progress);

		String s = String.format("%d:%02d / %d:%02d"
				, pos / 60, pos % 60, dur / 60, dur % 60);
		b.lpos.setText(s);
		return 0;
	}
}
