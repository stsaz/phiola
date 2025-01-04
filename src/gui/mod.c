/** phiola: GUI--Core bridge
2023, Simon Zolin */

#ifdef _WIN32
#include <util/windows-shell.h>
#else
#include <util/unix-shell.h>
#endif
#include <gui/mod.h>
#include <gui/track-playback.h>
#include <gui/track-convert.h>
#include <afilter/pcm.h>
#include <ffsys/dir.h>
#include <ffsys/globals.h>
#include <ffbase/args.h>

const phi_core *core;
struct gui_data *gd;

#define AUTO_LIST_FN  "list%u.m3uz"
#ifdef FF_WIN
	#define USER_CONF_DIR  "%APPDATA%\\phiola\\"
#else
	#define USER_CONF_DIR  "$HOME/.config/phiola/"
#endif
#define USER_CONF_NAME  "gui.conf"
#define USER_CONF_NAME_ALT  "user.conf"

static void list_filter_close();
static void gui_finish();

#define O(m)  (void*)FF_OFF(struct gui_data, m)
const struct ffarg guimod_args[] = {
	{ "list.auto_sel",	'u',	O(auto_select) },
	{ "play.auto_norm",	'u',	O(conf.auto_norm) },
	{ "play.auto_skip",	'd',	O(conf.auto_skip_sec_percent) },
	{ "play.cursor",	'u',	O(cursor) },
	{ "play.dev",		'u',	O(conf.odev) },
	{ "play.random",	'u',	O(conf.random) },
	{ "play.repeat",	'u',	O(conf.repeat) },
	{ "play.seek_leap",	'u',	O(conf.seek_leap_delta) },
	{ "play.seek_step",	'u',	O(conf.seek_step_delta) },
	{ "play.volume",	'u',	O(volume) },
	{ "theme",			'=s',	O(conf.theme) },
	{}
};
#undef O

void mod_userconf_write(ffconfw *cw)
{
	ffconfw_add2u(cw, "list.auto_sel", gd->auto_select);
	ffconfw_add2u(cw, "play.auto_norm", gd->conf.auto_norm);
	ffconfw_add2u(cw, "play.auto_skip", (ffint64)gd->conf.auto_skip_sec_percent);
	ffconfw_add2u(cw, "play.cursor", gd->cursor);
	ffconfw_add2u(cw, "play.dev", gd->conf.odev);
	ffconfw_add2u(cw, "play.random", gd->conf.random);
	ffconfw_add2u(cw, "play.repeat", gd->conf.repeat);
	ffconfw_add2u(cw, "play.seek_leap", gd->conf.seek_leap_delta);
	ffconfw_add2u(cw, "play.seek_step", gd->conf.seek_step_delta);
	ffconfw_add2u(cw, "play.volume", gd->volume);
	if (gd->conf.theme)
		ffconfw_add2z(cw, "theme", gd->conf.theme);
}

#ifdef FF_LINUX
static int file_del_trash(const char **names, ffsize n)
{
	ffsize err = 0;
	const char *e;
	for (ffsize i = 0;  i != n;  i++) {
		if (0 != ffui_glib_trash(names[i], &e)) {
			errlog("can't move file to trash: %s: %s"
				, names[i], e);
			err++;
		}
	}

	dbglog("moved %L files to Trash", n - err);
	return (err == 0) ? 0 : -1;
}
#endif

/** Trash all selected files */
void file_del(ffslice indices)
{
	if (gd->q_filtered) goto end;

	ffvec names = {};
	struct phi_queue_entry *qe;
	uint *it;

	FFSLICE_WALK(&indices, it) {
		qe = gd->queue->at(gd->q_selected, *it);
		*ffvec_pushT(&names, char*) = qe->url;
	}

#ifdef FF_WIN
	int r = ffui_file_del((const char *const *)names.ptr, names.len, FFUI_FILE_TRASH);
#else
	int r = file_del_trash((const char**)names.ptr, names.len);
#endif

	FFSLICE_RWALK(&indices, it) {
		qe = gd->queue->at(gd->q_selected, *it);
		gd->queue->remove(qe);
	}

	if (r == 0)
		wmain_status("Deleted %L files", names.len);
	else
		wmain_status("Couldn't delete some files");

	ffvec_free(&names);
end:
	ffslice_free(&indices);
}

#ifdef FF_LINUX
static void dir_show(const char *dir)
{
	const char *argv[] = { "xdg-open", dir, NULL };
	ffps ps = ffps_exec("/usr/bin/xdg-open", argv, (const char**)environ);
	if (ps == FFPS_NULL) {
		syserrlog("ffps_exec", 0);
		return;
	}

	dbglog("spawned file manager: %u", (int)ffps_id(ps));
	ffps_close(ps);
}
#endif

/** Open/explore directory in UI for the first selected file */
void file_dir_show(ffslice indices)
{
	uint *it;
	FFSLICE_WALK(&indices, it) {
		const struct phi_queue_entry *qe = gd->queue->at(list_id_visible(), *it);

#ifdef FF_WIN
		const char *const names[] = { qe->url };
		ffui_openfolder(names, 1);

#else
		ffstr dir;
		ffpath_splitpath_str(FFSTR_Z(qe->url), &dir, NULL);
		char *dirz = ffsz_dupstr(&dir);
		dir_show(dirz);
		ffmem_free(dirz);
#endif

		break;
	}
	ffslice_free(&indices);
}

/** Create a new queue */
static phi_queue_id list_new()
{
	list_filter_close();
	struct phi_queue_conf qc = {
		.name = ffsz_allocfmt("Playlist %u", ++gd->playlist_counter),
		.first_filter = gd->playback_first_filter,
		.ui_module = "gui.track",
	};
	gd->tab_conversion = 0;
	phi_queue_id q = gd->queue->create(&qc);
	return q;
}

/** Remember the current vertical scroll position */
static void list_scroll_store(phi_queue_id q, uint vpos)
{
	struct list_info *li;
	FFSLICE_WALK(&gd->lists, li) {
		if (li->q == q) {
			li->scroll_vpos = vpos;
			break;
		}
	}
}

/** A queue is created */
void list_created(phi_queue_id q)
{
	struct list_info *li = ffvec_zpushT(&gd->lists, struct list_info);
	li->q = q;
	phi_queue_id old = gd->q_selected;
	gd->q_selected = q;

	uint scroll_vpos = wmain_list_add(gd->queue->conf(q)->name, gd->lists.len - 1);

	list_scroll_store(old, scroll_vpos);
}

/** Close current list */
static void list_close()
{
	list_filter_close();
	uint play_lists_n = gd->lists.len - !!(gd->q_convert);

	if (gd->q_selected == gd->q_convert) {
		gd->queue->destroy(gd->q_selected);
		gd->q_convert = NULL;

	} else if (play_lists_n > 1) {
		gd->queue->destroy(gd->q_selected);

	} else {
		gd->queue->clear(gd->q_selected);
	}
}

/** A queue is deleted */
void list_deleted(phi_queue_id q)
{
	uint i = 0;
	struct list_info *li;
	FFSLICE_WALK(&gd->lists, li) {
		if (li->q == q)
			break;
		i++;
	}
	ffslice_rmT((ffslice*)&gd->lists, i, 1, struct list_info);

	wmain_list_delete(i);
}

/** Change current list */
void list_select(uint i)
{
	list_filter_close();
	struct list_info *li = ffslice_itemT(&gd->lists, i, struct list_info);
	gd->tab_conversion = (gd->q_convert == li->q);
	phi_queue_id old = gd->q_selected;
	gd->q_selected = li->q;
	uint n = gd->queue->count(gd->q_selected);

	list_scroll_store(old, gd->current_scroll_vpos);
	wmain_list_select(n, li->scroll_vpos);
}

/** Get currently visible (filtered) queue */
phi_queue_id list_id_visible()
{
	return (gd->q_filtered) ? gd->q_filtered : gd->q_selected;
}

/** Close filtered queue */
static void list_filter_close()
{
	if (!gd->q_filtered) return;

	gd->queue->destroy(gd->q_filtered);
	gd->q_filtered = NULL;
}

/** Filter the listing */
void list_filter(ffstr filter)
{
	if (gd->tab_conversion) goto end;

	phi_queue_id q = gd->q_selected;
	if (filter.len) {
		q = (gd->q_filtered && filter.len > gd->filter_len) ? gd->q_filtered : gd->q_selected;

		gd->filtering = 1;
		q = gd->queue->filter(q, filter, 0);
		gd->filtering = 0;

		if (gd->q_filtered)
			gd->queue->destroy(gd->q_filtered);
		gd->q_filtered = q;

		uint total = gd->queue->count(gd->q_selected);
		uint n = gd->queue->count(gd->q_filtered);
		wmain_status("Filter: %u/%u", n, total);

	} else {
		list_filter_close();
		wmain_status("");
	}

	gd->filter_len = filter.len;

	wmain_list_draw(gd->queue->count(q), 1);

end:
	ffstr_free(&filter);
}

void ctl_play(uint i)
{
	if (!gd->q_filtered && !gd->tab_conversion) {
		gd->queue->qselect(gd->q_selected); // set the visible list as default
		// Apply settings for the list that we're activating
		struct phi_queue_conf *qc = gd->queue->conf(NULL);
		qc->repeat_all = gd->conf.repeat;
		qc->random = gd->conf.random;
		qc->tconf.afilter.auto_normalizer = (gd->conf.auto_norm) ? "" : NULL;
		qc->tconf.oaudio.device_index = gd->conf.odev;
	}
	phi_queue_id q = (gd->q_filtered) ? gd->q_filtered : NULL;
	gd->queue->play(NULL, gd->queue->at(q, i));
}

void volume_set(uint vol)
{
	if (vol <= 100)
		gd->gain_db = vol2db(vol, 48);
	else
		gd->gain_db = vol2db_inc(vol - 100, 25, 6);
	gd->volume = vol;
}

void ctl_volume()
{
	if (gd->playing_track)
		gtrk_vol_apply(gd->playing_track, gd->gain_db);
}

static void ctl_seek(uint cmd)
{
	if (!gd->playing_track) return;

	int delta = 0, seek = 0;
	switch (cmd) {
	case A_SEEK:
		seek = gd->seek_pos_sec;  break;

	case A_STEP_FWD:
		delta = gd->conf.seek_step_delta;  break;

	case A_STEP_BACK:
		delta = -(int)gd->conf.seek_step_delta;  break;

	case A_LEAP_FWD:
		delta = gd->conf.seek_leap_delta;  break;

	case A_LEAP_BACK:
		delta = -(int)gd->conf.seek_leap_delta;  break;

	case A_MARKER_JUMP:
		if (gd->marker_sec == ~0U)
			return;
		seek = gd->marker_sec;
		break;
	}

	if (delta)
		seek = ffmax((int)gd->playing_track->last_pos_sec + delta, 0);

	gtrk_seek(gd->playing_track, seek);
}

void ctl_action(uint cmd)
{
	uint n;

	switch (cmd) {

	case A_LIST_NEW:
		list_new();  break;

	case A_LIST_CLOSE:
		list_close();  break;

	case A_LIST_CLEAR:
		if (!gd->q_filtered)
			gd->queue->clear(gd->q_selected);
		break;

	case A_LIST_SORT:
		n = PHI_Q_SORT_FILENAME;
		goto list_sort;

	case A_LIST_SORT_FILESIZE:
		n = PHI_Q_SORT_FILESIZE;
		goto list_sort;

	case A_LIST_SORT_FILEDATE:
		n = PHI_Q_SORT_FILEDATE;
		goto list_sort;

	case A_LIST_SHUFFLE:
		n = PHI_Q_SORT_RANDOM;
list_sort:
		if (!gd->q_filtered) {
			gd->queue->sort(gd->q_selected, n);
		}
		break;

	case A_LIST_REMOVE_NONEXIST:
		gd->queue->remove_multi(gd->q_selected, PHI_Q_RM_NONEXIST);  break;

	case A_PLAYPAUSE:
		if (gd->playing_track)
			gtrk_play_pause(gd->playing_track);
		else
			ctl_play(gd->cursor);
		break;

	case A_STOP:
		if (gd->playing_track)
			core->track->stop(gd->playing_track->t);
		break;

	case A_NEXT:
		gd->queue->play_next(NULL);  break;

	case A_PREV:
		gd->queue->play_previous(NULL);  break;

	case A_MARKER_SET:
		if (gd->playing_track) {
			gd->marker_sec = gd->playing_track->last_pos_sec;
			wmain_status("Marker: %u:%02u", gd->marker_sec / 60, gd->marker_sec % 60);
		}
		break;

	case A_SEEK:
	case A_STEP_FWD:
	case A_STEP_BACK:
	case A_LEAP_FWD:
	case A_LEAP_BACK:
	case A_MARKER_JUMP:
		ctl_seek(cmd);  break;

	case A_GOTO_SHOW:
		if (gd->playing_track)
			gui_task_uint(wgoto_show, gd->playing_track->last_pos_sec);
		break;

	case A_REPEAT_TOGGLE:
	case A_RANDOM_TOGGLE: {
		// Apply settings for the active playlist
		struct phi_queue_conf *qc = gd->queue->conf(NULL);
		int r;
		switch (cmd) {
		case A_REPEAT_TOGGLE:
			r = qc->repeat_all = gd->conf.repeat = !gd->conf.repeat;
			wmain_status("Repeat: %s", (r) ? "All" : "None");
			break;

		case A_RANDOM_TOGGLE:
			r = qc->random = gd->conf.random = !gd->conf.random;
			wmain_status("Random: %s", (r) ? "On" : "Off");
			break;
		}
	}
		break;
	}
}

/** Save user-config to disk */
void userconf_save(ffstr data)
{
	if (fffile_writewhole(gd->user_conf_name, data.ptr, data.len, 0))
		syserrlog("file write: %s", gd->user_conf_name);
	ffstr_free(&data);
}

/** Save the visible (not filtered) list */
void list_save(void *sz)
{
	char *fn = sz;
	gd->queue->save(gd->q_selected, fn, NULL, NULL);
	ffmem_free(fn);
}

static void list_save_complete(void *param, phi_track *t)
{
	if (--gd->list_save_pending == 0)
		gui_finish();
}

/** Save playlists to disk */
static void lists_save()
{
	if (!!ffdir_make(gd->user_conf_dir) && !fferr_exist(fferr_last()))
		syserrlog("dir make: %s", gd->user_conf_dir);

	char *fn = NULL;
	uint i = 1;
	struct list_info *li;
	FFSLICE_WALK(&gd->lists, li) {
		if (li->q == gd->q_convert)
			continue;

		ffmem_free(fn);
		fn = ffsz_allocfmt("%s" AUTO_LIST_FN, gd->user_conf_dir, i++);
		if (!gd->queue->save(li->q, fn, list_save_complete, NULL))
			gd->list_save_pending++;
	}

	ffmem_free(fn);
	fn = ffsz_allocfmt("%s" AUTO_LIST_FN, gd->user_conf_dir, i);
	fffile_remove(fn);

	ffmem_free(fn);
}

/** Load playlists from disk */
void lists_load()
{
	char *fn = NULL;
	for (uint i = 1;;  i++) {

		fn = ffsz_allocfmt("%s" AUTO_LIST_FN, gd->user_conf_dir, i);
		fffileinfo fi;
		if (fffile_info_path(fn, &fi))
			break;

		uint mt_set = 1;
		phi_queue_id q = NULL;
		if (i == 1) {
			// Don't set `last_mod_time` if there are tracks added from command line.
			// This prevents `m3u-read` from setting `modified=0` on the queue.
			mt_set = (gd->queue->count(q) == 0);
		} else {
			q = list_new();
		}

		if (mt_set) {
			fftime mt = fffileinfo_mtime(&fi);
			mt.sec += FFTIME_1970_SECONDS;
			gd->queue->conf(q)->last_mod_time = mt;
		}

		struct phi_queue_entry qe = {
			.url = fn,
		};
		fn = NULL;
		gd->queue->add(q, &qe);
	}

	ffmem_free(fn);
}

void list_add(ffstr fn)
{
	if (gd->q_filtered) return;

	struct phi_queue_entry qe = {
		.url = ffsz_dupstr(&fn),
	};
	gd->queue->add(gd->q_selected, &qe);
	ffmem_free(qe.url);
}

void list_add_sz(void *sz)
{
	if (gd->q_filtered)
		goto end;

	struct phi_queue_entry qe = {
		.url = sz,
	};
	gd->queue->add(gd->q_selected, &qe);

end:
	ffmem_free(sz);
}

void list_add_multi(ffslice names)
{
	char **it;
	FFSLICE_WALK(&names, it) {
		list_add_sz(*it);
	}
	ffslice_free(&names);
}

/** Get next playback list (skip conversion list) */
static phi_queue_id list_next_playback()
{
	uint i = 0;
	struct list_info *li;
	FFSLICE_WALK(&gd->lists, li) {
		if (li->q == gd->q_selected)
			break;
		i++;
	}

	i++;
	if (i == gd->lists.len)
		return NULL;
	li = ffslice_itemT(&gd->lists, i, struct list_info);

	if (li->q == gd->q_convert) {
		i++;
		if (i == gd->lists.len)
			return NULL;
		li = ffslice_itemT(&gd->lists, i, struct list_info);
	}

	return li->q;
}

/** Add selected tracks to the next playback list */
void list_add_to_next(ffslice indices)
{
	phi_queue_id q_target = list_next_playback();
	if (!q_target)
		goto end;

	phi_queue_id q_src = list_id_visible();
	uint *it;
	FFSLICE_WALK(&indices, it) {
		struct phi_queue_entry *qe = gd->queue->at(q_src, *it), nqe = {};
		qe_copy(&nqe, qe, gd->metaif);
		gd->queue->add(q_target, &nqe);
	}

end:
	ffslice_free(&indices);
}

void list_remove(ffslice indices)
{
	if (gd->q_filtered) goto end;

	uint *it;
	FFSLICE_RWALK(&indices, it) {
		gd->queue->remove(gd->queue->at(gd->q_selected, *it));
	}

end:
	ffslice_free(&indices);
}


static void grd_rec_close(void *f, phi_track *t)
{
	core->track->stop(t);
	gd->recording_track = NULL;
	if (gd->quit) {
		gui_finish();
		return;
	}
	gui_task(wrecord_done);
}
static int grd_rec_process(void *f, phi_track *t) { return PHI_DONE; }
static const phi_filter phi_gui_record_guard = {
	NULL, grd_rec_close, grd_rec_process,
	"record-guard"
};

void record_begin(void *param)
{
	struct phi_track_conf *c = param;
	phi_track *t = core->track->create(c);
	int e = 1;

	if (!core->track->filter(t, &phi_gui_record_guard, 0)
		|| !core->track->filter(t, core->mod("core.auto-rec"), 0)
		|| !core->track->filter(t, core->mod("afilter.until"), 0)
		|| !core->track->filter(t, core->mod("afilter.gain"), 0)
		|| !core->track->filter(t, core->mod("afilter.auto-conv"), 0)
		|| !core->track->filter(t, core->mod("format.auto-write"), 0)
		|| !core->track->filter(t, core->mod("core.file-write"), 0))
		goto end;

	gd->recording_track = t;
	core->track->start(t);
	e = 0;

end:
	if (e) {
		core->track->close(t);
	}
	ffmem_free(c);
}

int record_stop()
{
	if (!gd->recording_track) return -1;

	core->track->stop(gd->recording_track);
	return 0;
}


static void grd_conv_close(void *f, phi_track *t)
{
	core->track->stop(t);
}
static int grd_conv_process(void *f, phi_track *t) { return PHI_DONE; }
static const phi_filter phi_gui_convert_guard = {
	NULL, grd_conv_close, grd_conv_process,
	"convert-guard"
};

/** Create conversion queue and add tracks to it */
void convert_add(ffslice indices)
{
	phi_queue_id q = list_id_visible();

	if (!gd->q_convert) {
		struct phi_queue_conf qc = {
			.name = ffsz_dup("Conversion"),
			.first_filter = &phi_gui_convert_guard,
			.ui_module = "gui.track-convert",
			.conversion = 1,
		};
		gd->tab_conversion = 1;
		gd->q_convert = gd->queue->create(&qc); // q_on_change('n') adds a tab
	}

	uint *it;
	FFSLICE_WALK(&indices, it) {
		const struct phi_queue_entry *iqe = gd->queue->at(q, *it);

		struct phi_queue_entry qe = {};
		qe_copy(&qe, iqe, gd->metaif);
		gd->queue->add(gd->q_convert, &qe);
	}

	ffslice_free(&indices);
}

/** Set config for each track and begin conversion */
void convert_begin(void *param)
{
	struct phi_track_conf *c = param;
	struct phi_queue_entry *qe = NULL;
	if (!gd->q_convert)
		goto end;

	if ((qe = gd->queue->at(gd->q_convert, 0))) {
		struct phi_queue_conf *qc = gd->queue->conf(gd->q_convert);
		gd->metaif->destroy(&qc->tconf.meta);
		ffmem_free(qc->tconf.ofile.name);
		qc->tconf = *c;
		phi_meta_null(&c->meta);
		c->ofile.name = NULL;
		gd->queue->play(NULL, gd->queue->at(gd->q_convert, 0));
	}

end:
	if (!qe) {
		wconvert_done();
		ffmem_free(c->ofile.name);
		gd->metaif->destroy(&c->meta);
	}
	ffmem_free(c);
}


const phi_adev_if* adev_find_mod()
{
	if (gd->adev_if) return gd->adev_if;

	static const char mods[][20] = {
#if defined FF_WIN
		"wasapi.dev",
#else
		"pulse.dev",
		"alsa.dev",
#endif
	};
	for (uint i = 0;  i < FF_COUNT(mods);  i++) {
		const void *f;
		if ((f = core->mod(mods[i]))) {
			gd->adev_if = f;
			return f;
		}
	}
	return NULL;
}


struct cmd {
	phi_task task;
	uint flags;
	union {
	void (*func)();
	void (*func_uint)(uint);
	void (*func_ptr)(void*);
	void (*func_ffstr)(ffstr);
	};
	union {
	ffstr data;
	uint data_uint;
	void *data_ptr;
	};
};

static void corecmd_handler(void *param)
{
	struct cmd *c = param;
	dbglog("%s %p", __func__, c->func);
	switch (c->flags & 3) {
	case 0:
		c->func();  break;
	case 1:
		c->func_uint(c->data_uint);  break;
	case 2:
		c->func_ptr(c->data_ptr);  break;
	case 3:
		c->func_ffstr(c->data);  break;
	}
	ffmem_free(c);
}

void gui_core_task(void (*func)())
{
	struct cmd *c = ffmem_new(struct cmd);
	c->flags = 0;
	c->func = func;
	core->task(0, &c->task, corecmd_handler, c);
}

void gui_core_task_uint(void (*func)(uint), uint i)
{
	struct cmd *c = ffmem_new(struct cmd);
	c->flags = 1;
	c->func_uint = func;
	c->data_uint = i;
	core->task(0, &c->task, corecmd_handler, c);
}

void gui_core_task_ptr(void (*func)(void*), void *ptr)
{
	struct cmd *c = ffmem_new(struct cmd);
	c->flags = 2;
	c->func_ptr = func;
	c->data_ptr = ptr;
	core->task(0, &c->task, corecmd_handler, c);
}

void gui_core_task_data(void (*func)(ffstr), ffstr d)
{
	struct cmd *c = ffmem_new(struct cmd);
	c->flags = 3;
	c->func_ffstr = func;
	c->data = d;
	core->task(0, &c->task, corecmd_handler, c);
}

static void gui_start(void *param)
{
	gd->conf.seek_step_delta = 10;
	gd->conf.seek_leap_delta = 60;
	gd->marker_sec = ~0;
	gd->volume = 100;
	gd->queue = core->mod("core.queue");
	gd->metaif = core->mod("format.meta");

	char *user_conf_fn = ffsz_allocfmt("%Smod/gui/%s", &core->conf.root, USER_CONF_NAME_ALT);
	if (fffile_exists(user_conf_fn)) {
		gd->user_conf_dir = ffsz_allocfmt("%Smod/gui/", &core->conf.root);
		gd->user_conf_name = user_conf_fn;
	} else {
		ffmem_free(user_conf_fn);
		gd->user_conf_dir = core->conf.env_expand(USER_CONF_DIR);
		gd->user_conf_name = ffsz_allocfmt("%s%s", gd->user_conf_dir, USER_CONF_NAME);
	}

	gui_init();
	gui_userconf_load();
	volume_set(gd->volume);
	if (FFTHREAD_NULL == (gd->th = ffthread_create(gui_worker, NULL, 0)))
		return;

	phi_queue_id q = gd->queue->select(0);

	struct phi_queue_conf *qc = gd->queue->conf(q);
	gd->playback_first_filter = qc->first_filter;
	qc->name = ffsz_dup("Playlist 1");
	qc->repeat_all = gd->conf.repeat;
	qc->random = gd->conf.random;
	qc->tconf.afilter.auto_normalizer = (gd->conf.auto_norm) ? "" : NULL;
	qc->tconf.oaudio.device_index = gd->conf.odev;

	struct list_info *li = ffvec_zpushT(&gd->lists, struct list_info);
	li->q = q;
	gd->q_selected = q;
	gd->playlist_counter = 1;
	if (gd->queue->count(q))
		ctl_play(0);
}

void gui_stop(uint flags)
{
	gd->ui_thread_busy = !!flags;
	gd->quit = 1;
	lists_save();
	record_stop();
	gui_finish();
}

static void gui_finish()
{
	if (gd->recording_track
		|| gd->list_save_pending)
		return;

	core->sig(PHI_CORE_STOP);
}

static void gui_destroy()
{
	if (!gd->ui_thread_busy)
		ffthread_join(gd->th, -1, NULL);
	ffmem_free(gd->user_conf_dir);
	ffvec_free(&gd->lists);
	ffmem_free(gd);
}


phi_log_ctl gui_log_ctl;
static void gui_conf(struct phi_ui_conf *c)
{
	gui_log_ctl = c->log_ctl;
}

extern void gui_log(void *udata, ffstr s);
static const phi_ui_if phi_gui_if = {
	.conf = gui_conf,
	.log = gui_log,
};


extern const phi_filter phi_gui_track;
static const void* gui_iface(const char *name)
{
	static const struct map_sz_vptr ifs[] = {
		{"if",				&phi_gui_if},
		{"track",			&phi_gui_track},
		{"track-convert",	&phi_gui_conv},
	};
	return map_sz_vptr_findz2(ifs, FF_COUNT(ifs), name);
}

static const phi_mod phi_gui_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	gui_iface,
	gui_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	gd = ffmem_new(struct gui_data);
	core->task(0, &gd->task, gui_start, NULL);
	return &phi_gui_mod;
}
