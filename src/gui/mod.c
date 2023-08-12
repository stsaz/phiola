/** phiola: GUI--Core bridge
2023, Simon Zolin */

#ifdef _WIN32
#include <util/windows-shell.h>
#else
#include <util/unix-shell.h>
#endif
#include <gui/mod.h>
#include <gui/track.h>
#include <FFOS/dir.h>
#include <FFOS/ffos-extern.h>

const phi_core *core;
struct gui_data *gd;

#define AUTO_LIST_FN  "list%u.m3uz"
#ifdef FF_WIN
	#define USER_CONF_DIR  "%APPDATA%/phiola/"
#else
	#define USER_CONF_DIR  "$HOME/.config/phiola/"
#endif
#define USER_CONF_NAME  "gui.conf"

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
void file_del(ffstr data)
{
	ffslice indexes = *(ffslice*)&data;
	ffvec names = {};
	struct phi_queue_entry *qe;
	uint *it;

	FFSLICE_WALK(&indexes, it) {
		qe = gd->queue->at(NULL, *it);
		*ffvec_pushT(&names, char*) = qe->conf.ifile.name;
	}

#ifdef FF_WIN
	int r = ffui_fop_del((const char *const *)names.ptr, names.len, FFUI_FOP_ALLOWUNDO);
#else
	int r = file_del_trash((const char**)names.ptr, names.len);
#endif

	FFSLICE_RWALK(&indexes, it) {
		qe = gd->queue->at(NULL, *it);
		gd->queue->remove(qe);
	}

	if (r == 0)
		wmain_status("Deleted %L files", names.len);
	else
		wmain_status("Couldn't delete some files");

	ffvec_free(&names);
	ffslice_free(&indexes);
}

void ctl_play(uint i)
{
	gd->queue->play(NULL, gd->queue->at(NULL, i));
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
	}

	if (delta)
		seek = ffmax((int)gd->playing_track->last_pos_sec + delta, 0);

	gtrk_seek(gd->playing_track, seek);
}

void ctl_action(uint cmd)
{
	switch (cmd) {
	case A_LIST_CLEAR:
		gd->queue->clear(NULL);  break;

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

	case A_SEEK:
	case A_STEP_FWD:
	case A_STEP_BACK:
	case A_LEAP_FWD:
	case A_LEAP_BACK:
		ctl_seek(cmd);  break;
	}
}

/** Save user-config to disk */
void userconf_save(ffstr data)
{
	char *fn = ffsz_allocfmt("%s%s", gd->user_conf_dir, USER_CONF_NAME);
	if (!!fffile_writewhole(fn, data.ptr, data.len, 0))
		syserrlog("file write: %s", fn);
	ffmem_free(fn);
	ffstr_free(&data);
}

/** Save playlists to disk */
void lists_save()
{
	if (!!ffdir_make(gd->user_conf_dir) && !fferr_exist(fferr_last()))
		syserrlog("dir make: %s", gd->user_conf_dir);

	char *fn = ffsz_allocfmt("%s" AUTO_LIST_FN, gd->user_conf_dir, 1);
	gd->queue->save(NULL, fn);
	ffmem_free(fn);
}

/** Load playlists from disk */
static void lists_load()
{
	struct phi_queue_entry qe = {
		.conf.ifile.name = ffsz_allocfmt("%s" AUTO_LIST_FN, gd->user_conf_dir, 1),
	};
	gd->queue->add(NULL, &qe);
}

void list_add(ffstr fn)
{
	struct phi_queue_entry qe = {};
	qe.conf.ifile.name = ffsz_dupstr(&fn);
	gd->queue->add(NULL, &qe);
}

void list_add_sz(void *sz)
{
	struct phi_queue_entry qe = {
		.conf.ifile.name = sz,
	};
	gd->queue->add(NULL, &qe);
}

void list_remove(ffstr data)
{
	ffslice d = *(ffslice*)&data;
	uint *it;
	FFSLICE_RWALK(&d, it) {
		gd->queue->remove(gd->queue->at(NULL, *it));
	}
	ffslice_free(&d);
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
	core->task(&c->task, corecmd_handler, c);
}

void gui_core_task_uint(void (*func)(uint), uint i)
{
	struct cmd *c = ffmem_new(struct cmd);
	c->func_uint = func;
	c->data_uint = i;
	core->task(&c->task, corecmd_handler, c);
}

void gui_core_task_ptr(void (*func)(void*), void *ptr)
{
	struct cmd *c = ffmem_new(struct cmd);
	c->func_ptr = func;
	c->data_ptr = ptr;
	core->task(&c->task, corecmd_handler, c);
}

void gui_core_task_data(void (*func)(ffstr), ffstr d)
{
	struct cmd *c = ffmem_new(struct cmd);
	c->func_ffstr = func;
	c->data = d;
	core->task(&c->task, corecmd_handler, c);
}

static void gui_start(void *param)
{
	gd->conf.seek_step_delta = 10;
	gd->conf.seek_leap_delta = 60;
	gd->volume = 100;
	gd->queue = core->mod("core.queue");
	gd->metaif = core->mod("format.meta");
	gd->user_conf_dir = core->conf.env_expand(USER_CONF_DIR);

	lists_load();

	if (FFTHREAD_NULL == (gd->th = ffthread_create(gui_worker, NULL, 0)))
		return;
}

void gui_stop()
{
	core->sig(PHI_CORE_STOP);
}

static void gui_destroy()
{
	ffthread_join(gd->th, -1, NULL);
	ffmem_free(gd->user_conf_dir);
	ffmem_free(gd);
}

extern const phi_filter phi_gui_track;
static const void* gui_iface(const char *name)
{
	if (ffsz_eq(name, "track")) return &phi_gui_track;
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
	core->task(&gd->task, gui_start, NULL);
	return &phi_gui_mod;
}
