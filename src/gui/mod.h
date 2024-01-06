/** phiola: GUI
2023, Simon Zolin */

#include <ffsys/error.h>
#include <phiola.h>
#include <util/conf-write.h>
#include <ffsys/thread.h>

FF_EXTERN const phi_core *core;

FF_EXTERN int FFTHREAD_PROCCALL gui_worker(void *param);

/** Execute commands within Core thread */
FF_EXTERN void gui_core_task(void (*func)());
FF_EXTERN void gui_core_task_uint(void (*func)(uint), uint i);
FF_EXTERN void gui_core_task_ptr(void (*func)(void*), void *ptr);
FF_EXTERN void gui_core_task_data(void (*func)(ffstr), ffstr d);
static inline void gui_core_task_slice(void (*func)(ffslice), ffslice d) {
	gui_core_task_data((void (*)(ffstr))(void*)func, *(ffstr*)&d);
}

/** Execute commands within GUI thread */
FF_EXTERN void gui_task_ptr(void (*func)(void*), void *ptr);
static inline void gui_task_uint(void (*func)(uint), uint i) {
	gui_task_ptr((void(*)(void*))(void*)func, (void*)(ffsize)i);
}
static inline void gui_task(void (*func)()) {
	gui_task_ptr((void(*)(void*))(void*)func, NULL);
}

FF_EXTERN void file_dir_show(ffslice indexes);

FF_EXTERN void list_created(phi_queue_id q);
FF_EXTERN void list_add(ffstr fn);
FF_EXTERN void list_add_sz(void *sz);
FF_EXTERN void list_add_multi(ffslice names);
FF_EXTERN void list_add_to_next(ffslice indexes);
FF_EXTERN void list_remove(ffslice indexes);
FF_EXTERN void list_save(void *sz);
FF_EXTERN void lists_save();
FF_EXTERN void lists_load();
FF_EXTERN void list_deleted(phi_queue_id q);
FF_EXTERN void list_select(uint i);
FF_EXTERN void list_filter(ffstr filter);
FF_EXTERN phi_queue_id list_id_visible();

FF_EXTERN void ctl_play(uint i);
FF_EXTERN void volume_set(uint vol);
FF_EXTERN void ctl_volume();
FF_EXTERN void ctl_action(uint id);
FF_EXTERN void mod_userconf_write(ffconfw *cw);
FF_EXTERN void userconf_save(ffstr data);
FF_EXTERN void gui_stop();

FF_EXTERN void record_begin(void *param);
FF_EXTERN int record_stop();

FF_EXTERN void convert_add(ffslice indexes);
FF_EXTERN void convert_begin(void *param);

FF_EXTERN const phi_adev_if* adev_find_mod();

FF_EXTERN void wmain_status(const char *fmt, ...);
enum STATUS_ID {
	ST_STOPPED,
	ST_PAUSED,
	ST_UNPAUSED,
};
/**
id: enum STATUS_ID */
FF_EXTERN void wmain_status_id(uint id);
FF_EXTERN int wmain_track_new(phi_track *t, uint time_total);
FF_EXTERN void wmain_track_close();
FF_EXTERN void wmain_track_update(uint time_cur, uint time_total);
FF_EXTERN void wmain_conv_track_new(phi_track *t, uint time_total);
FF_EXTERN void wmain_conv_track_close(phi_track *t);
FF_EXTERN void wmain_conv_track_update(phi_track *t, uint time_cur, uint time_total);

FF_EXTERN uint wmain_list_add(const char *name, uint i);
FF_EXTERN void wmain_list_delete(uint i);
FF_EXTERN void wmain_list_select(uint n, uint scroll_vpos);
FF_EXTERN void wmain_list_draw(uint n, uint flags);

FF_EXTERN void wgoto_show(uint pos);

FF_EXTERN void wrecord_done();

FF_EXTERN void wconvert_done();

struct gtrk;

struct list_info {
	phi_queue_id q;
	uint scroll_vpos;
};

struct gui_data {
	const phi_queue_if *queue;
	const phi_meta_if *metaif;
	char *user_conf_dir;
	char *user_conf_name;
	const phi_filter *playback_first_filter;
	const phi_adev_if *adev_if;

	struct gtrk *playing_track;
	uint cursor;
	uint seek_pos_sec;
	uint marker_sec;

	phi_track *recording_track;

	uint volume;
	double gain_db;

	phi_task task;
	ffthread th;

	ffvec lists; // struct list_info[]
	uint playlist_counter;
	phi_queue_id q_selected; // Currently visible list
	phi_queue_id q_convert;

	/**
	Uses the same tab as parent; auto-reset on list change.
	The only operations allowed: "play at pos", "add to convert list". */
	phi_queue_id q_filtered;

	uint filter_len; // Length of the current filter text
	uint current_scroll_vpos;
	uint auto_select;
	uint tab_conversion :1;
	uint filtering :1;
	uint quit :1;

	struct {
		char*	theme;
		uint	odev;
		uint	repeat;
		uint	random;
		uint	seek_step_delta;
		uint	seek_leap_delta;
	} conf;
};
FF_EXTERN struct gui_data *gd;

static inline void playback_device_set() { gd->queue->device(gd->conf.odev); }

#define syserrlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR | PHI_LOG_SYS, "gui", NULL, __VA_ARGS__)

#define errlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR, "gui", NULL, __VA_ARGS__)

#define warnlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR, "gui", NULL, __VA_ARGS__)

#define dbglog(...) \
do { \
	if (core->conf.log_level >= PHI_LOG_DEBUG) \
		core->conf.log(core->conf.log_obj, PHI_LOG_DEBUG, "gui", NULL, __VA_ARGS__); \
} while (0)

enum ACTION {
	A_NONE,

	#define X(id)  id
	#include "actions.h"
	#undef X

	// private:
	A_CLOSE,
	A_FILE_DRAGDROP,
	A_LIST_DISPLAY,
};
