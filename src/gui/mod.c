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
	#define USER_CONF_DIR  "%APPDATA%/phiola/"
#else
	#define USER_CONF_DIR  "$HOME/.config/phiola/"
#endif
#define USER_CONF_NAME  "gui.conf"
#define USER_CONF_NAME_ALT  "user.conf"

static void list_filter_close();

#define O(m)  (void*)FF_OFF(struct gui_data, m)
const struct ffarg guimod_args[] = {
	{ "list.auto_sel",	'u',	O(auto_select) },
	{ "play.cursor",	'u',	O(cursor) },
	{ "play.random",	'u',	O(conf.random) },
	{ "play.repeat",	'u',	O(conf.repeat) },
	{ "play.seek_leap",	'u',	O(conf.seek_leap_delta) },
	{ "play.seek_step",	'u',	O(conf.seek_step_delta) },
	{ "theme",			'=s',	O(theme) },
	{}
};
#undef O

void mod_userconf_write(ffconfw *cw)
{
	ffconfw_add2u(cw, "list.auto_sel", gd->auto_select);
	ffconfw_add2u(cw, "play.cursor", gd->cursor);
	ffconfw_add2u(cw, "play.random", gd->conf.random);
	ffconfw_add2u(cw, "play.repeat", gd->conf.repeat);
	ffconfw_add2u(cw, "play.seek_leap", gd->conf.seek_leap_delta);
	ffconfw_add2u(cw, "play.seek_step", gd->conf.seek_step_delta);
	if (gd->theme)
		ffconfw_add2z(cw, "theme", gd->theme);
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
void file_del(ffslice indexes)
{
	if (gd->q_filtered) goto end;

	ffvec names = {};
	struct phi_queue_entry *qe;
	uint *it;

	FFSLICE_WALK(&indexes, it) {
		qe = gd->queue->at(gd->q_selected, *it);
		*ffvec_pushT(&names, char*) = qe->conf.ifile.name;
	}

#ifdef FF_WIN
	int r = ffui_fop_del((const char *const *)names.ptr, names.len, FFUI_FOP_ALLOWUNDO);
#else
	int r = file_del_trash((const char**)names.ptr, names.len);
#endif

	FFSLICE_RWALK(&indexes, it) {
		qe = gd->queue->at(gd->q_selected, *it);
		gd->queue->remove(qe);
	}

	if (r == 0)
		wmain_status("Deleted %L files", names.len);
	else
		wmain_status("Couldn't delete some files");

	ffvec_free(&names);
end:
	ffslice_free(&indexes);
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
void file_dir_show(ffslice indexes)
{
	uint *it;
	FFSLICE_WALK(&indexes, it) {
		const struct phi_queue_entry *qe = gd->queue->at(list_id_visible(), *it);

#ifdef FF_WIN
		const char *const names[] = { qe->conf.ifile.name };
		ffui_openfolder(names, 1);

#else
		ffstr dir;
		ffpath_splitpath_str(FFSTR_Z(qe->conf.ifile.name), &dir, NULL);
		char *dirz = ffsz_dupstr(&dir);
		dir_show(dirz);
		ffmem_free(dirz);
#endif

		break;
	}
	ffslice_free(&indexes);
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
		struct phi_queue_conf *qc = gd->queue->conf(NULL);
		qc->repeat_all = gd->conf.repeat;
		qc->random = gd->conf.random;
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

/** Toggle 'repeat all/none' setting for the default playlist */
static void list_repeat_toggle()
{
	struct phi_queue_conf *qc = gd->queue->conf(NULL);
	gd->conf.repeat = !gd->conf.repeat;
	qc->repeat_all = gd->conf.repeat;
	wmain_status("Repeat: %s", (gd->conf.repeat) ? "All" : "None");
}

/** Toggle 'random on/off' setting for the default playlist */
static void list_random_toggle()
{
	struct phi_queue_conf *qc = gd->queue->conf(NULL);
	gd->conf.random = !gd->conf.random;
	qc->random = gd->conf.random;
	wmain_status("Random: %s", (gd->conf.random) ? "On" : "Off");
}

void ctl_action(uint cmd)
{
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
	case A_LIST_SHUFFLE:
		if (!gd->q_filtered) {
			gd->queue->sort(gd->q_selected, (cmd == A_LIST_SHUFFLE) ? PHI_Q_SORT_RANDOM : 0);
			wmain_list_draw(gd->queue->count(gd->q_selected), 1);
		}
		break;

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
		list_repeat_toggle();  break;

	case A_RANDOM_TOGGLE:
		list_random_toggle();  break;
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

/** Save playlists to disk */
void lists_save()
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
		gd->queue->save(li->q, fn, NULL, NULL);
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
		if (!fffile_exists(fn))
			break;

		phi_queue_id q = NULL;
		if (i > 1)
			q = list_new();

		struct phi_queue_entry qe = {
			.conf.ifile.name = fn,
		};
		fn = NULL;
		gd->queue->add(q, &qe);
	}

	ffmem_free(fn);
}

void list_add(ffstr fn)
{
	if (gd->q_filtered) return;

	struct phi_queue_entry qe = {};
	qe.conf.ifile.name = ffsz_dupstr(&fn);
	gd->queue->add(gd->q_selected, &qe);
}

void list_add_sz(void *sz)
{
	if (gd->q_filtered) {
		ffmem_free(sz);
		return;
	}

	struct phi_queue_entry qe = {
		.conf.ifile.name = sz,
	};
	gd->queue->add(gd->q_selected, &qe);
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
void list_add_to_next(ffslice indexes)
{
	phi_queue_id q_target = list_next_playback();
	if (!q_target)
		goto end;

	phi_queue_id q_src = list_id_visible();
	uint *it;
	FFSLICE_WALK(&indexes, it) {
		struct phi_queue_entry *qe = gd->queue->at(q_src, *it), nqe = {};
		nqe.conf.ifile.name = ffsz_dup(qe->conf.ifile.name);
		gd->queue->add(q_target, &nqe);
	}

end:
	ffslice_free(&indexes);
}

void list_remove(ffslice indexes)
{
	if (gd->q_filtered) goto end;

	uint *it;
	FFSLICE_RWALK(&indexes, it) {
		gd->queue->remove(gd->queue->at(gd->q_selected, *it));
	}

end:
	ffslice_free(&indexes);
}


static void grd_rec_close(void *f, phi_track *t)
{
	core->track->stop(t);
	gd->recording_track = NULL;
	if (gd->quit) {
		core->sig(PHI_CORE_STOP);
		return;
	}
	wrecord_done();
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
	if (!gd->queue->status(gd->q_convert))
		wconvert_done();
}
static int grd_conv_process(void *f, phi_track *t) { return PHI_DONE; }
static const phi_filter phi_gui_convert_guard = {
	NULL, grd_conv_close, grd_conv_process,
	"convert-guard"
};

/** Create conversion queue and add tracks to it */
void convert_add(ffslice indexes)
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
	FFSLICE_WALK(&indexes, it) {
		const struct phi_queue_entry *iqe = gd->queue->at(q, *it);

		struct phi_queue_entry qe = {};
		phi_track_conf_assign(&qe.conf, &iqe->conf);
		qe.conf.ifile.name = ffsz_dup(iqe->conf.ifile.name);
		gd->metaif->copy(&qe.conf.meta, &iqe->conf.meta);
		gd->queue->add(gd->q_convert, &qe);
	}

	ffslice_free(&indexes);
}

/** Set config for each track and begin conversion */
void convert_begin(void *param)
{
	struct phi_track_conf *c = param;
	uint i = 0;
	if (!gd->q_convert)
		goto end;

	struct phi_queue_entry *qe;
	for (i = 0;  !!(qe = gd->queue->at(gd->q_convert, i));  i++) {
		qe->conf.ofile.name = ffsz_dup(c->ofile.name);
		qe->conf.seek_msec = c->seek_msec;
		qe->conf.until_msec = c->until_msec;
		qe->conf.aac.quality = c->aac.quality;
		qe->conf.vorbis.quality = c->vorbis.quality;
		qe->conf.opus.bitrate = c->opus.bitrate;
		qe->conf.stream_copy = c->stream_copy;
	}
	if (i)
		gd->queue->play(NULL, gd->queue->at(gd->q_convert, 0));

end:
	if (!i)
		wconvert_done();
	ffmem_free(c->ofile.name);
	ffmem_free(c);
}


struct cmd {
	phi_task task;
	void (*func)();
	void (*func_uint)(uint);
	void (*func_ptr)(void*);
	void (*func_ffstr)(ffstr);
	ffstr data;
	uint data_uint;
	void *data_ptr;
};

static void corecmd_handler(void *param)
{
	struct cmd *c = param;
	if (c->func)
		c->func();
	else if (c->func_ptr)
		c->func_ptr(c->data_ptr);
	else if (c->func_uint)
		c->func_uint(c->data_uint);
	else if (c->func_ffstr)
		c->func_ffstr(c->data);
	ffmem_free(c);
}

void gui_core_task(void (*func)())
{
	struct cmd *c = ffmem_new(struct cmd);
	c->func = func;
	core->task(0, &c->task, corecmd_handler, c);
}

void gui_core_task_uint(void (*func)(uint), uint i)
{
	struct cmd *c = ffmem_new(struct cmd);
	c->func_uint = func;
	c->data_uint = i;
	core->task(0, &c->task, corecmd_handler, c);
}

void gui_core_task_ptr(void (*func)(void*), void *ptr)
{
	struct cmd *c = ffmem_new(struct cmd);
	c->func_ptr = func;
	c->data_ptr = ptr;
	core->task(0, &c->task, corecmd_handler, c);
}

void gui_core_task_data(void (*func)(ffstr), ffstr d)
{
	struct cmd *c = ffmem_new(struct cmd);
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

	phi_queue_id q = gd->queue->select(0);
	struct phi_queue_conf *qc = gd->queue->conf(q);
	gd->playback_first_filter = qc->first_filter;
	qc->name = ffsz_dup("Playlist 1");
	struct list_info *li = ffvec_zpushT(&gd->lists, struct list_info);
	li->q = q;
	gd->q_selected = q;
	gd->playlist_counter = 1;

	if (FFTHREAD_NULL == (gd->th = ffthread_create(gui_worker, NULL, 0)))
		return;
}

void gui_stop()
{
	gd->quit = 1;
	if (record_stop())
		core->sig(PHI_CORE_STOP);
}

static void gui_destroy()
{
	ffthread_join(gd->th, -1, NULL);
	ffmem_free(gd->user_conf_dir);
	ffvec_free(&gd->lists);
	ffmem_free(gd);
}

extern const phi_filter phi_gui_track;
extern void phi_gui_log(ffstr s);
static const void* gui_iface(const char *name)
{
	if (ffsz_eq(name, "track")) return &phi_gui_track;
	else if (ffsz_eq(name, "track-convert")) return &phi_gui_conv;
	else if (ffsz_eq(name, "log")) return &phi_gui_log;
	return NULL;
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
