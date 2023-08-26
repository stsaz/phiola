/** phiola: GUI
2023, Simon Zolin */

#include <FFOS/error.h>
#include <phiola.h>
#include <FFOS/thread.h>

FF_EXTERN const phi_core *core;

FF_EXTERN int FFTHREAD_PROCCALL gui_worker(void *param);

FF_EXTERN void gui_core_task(void (*func)());
FF_EXTERN void gui_core_task_uint(void (*func)(uint), uint i);
FF_EXTERN void gui_core_task_ptr(void (*func)(void*), void *ptr);
FF_EXTERN void gui_core_task_data(void (*func)(ffstr), ffstr d);
static inline void gui_core_task_slice(void (*func)(ffstr), ffslice d) {
	gui_core_task_data(func, *(ffstr*)&d);
}

FF_EXTERN void file_dir_show(ffstr data);
FF_EXTERN void list_add(ffstr fn);
FF_EXTERN void list_add_sz(void *sz);
FF_EXTERN void list_remove(ffstr data);
FF_EXTERN void lists_save();
FF_EXTERN void ctl_play(uint i);
FF_EXTERN void ctl_volume();
FF_EXTERN void ctl_action(uint id);
FF_EXTERN void userconf_save(ffstr data);
FF_EXTERN void gui_stop();

FF_EXTERN void convert_add(ffstr data);
FF_EXTERN void convert_begin(void *param);

FF_EXTERN void wmain_status(const char *fmt, ...);
FF_EXTERN int wmain_track_new(phi_track *t, uint time_total);
FF_EXTERN void wmain_track_close();
FF_EXTERN void wmain_track_update(uint time_cur, uint time_total);
FF_EXTERN void wmain_conv_track_new(phi_track *t, uint time_total);
FF_EXTERN void wmain_conv_track_close(phi_track *t);
FF_EXTERN void wmain_conv_track_update(phi_track *t, uint time_cur, uint time_total);

FF_EXTERN void wconvert_done();

struct gtrk;

struct gui_data {
	const phi_queue_if *queue;
	const phi_meta_if *metaif;
	char *user_conf_dir;

	struct gtrk *playing_track;
	uint cursor;
	uint seek_pos_sec;

	uint volume;
	double gain_db;

	fftask task;
	ffthread th;

	phi_queue_id q_selected, q_convert;
	uint tab_conversion :1;

	struct {
		uint seek_step_delta;
		uint seek_leap_delta;
	} conf;
};
FF_EXTERN struct gui_data *gd;

#define syserrlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR | PHI_LOG_SYS, "gui", NULL, __VA_ARGS__)

#define errlog(...) \
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
