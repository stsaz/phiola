/** phiola: TUI-ncurses
2026, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <util/aformat.h>
#include <afilter/pcm.h>
#include <util/ffncurses.h>
#include <ffsys/std.h>
#include <ffsys/pipe.h>
#include <ffsys/dirscan.h>
#include <ffsys/globals.h>

#define syserrlog(...)  phi_syserrlog(core, "tui2", NULL, __VA_ARGS__)
#define errlog(...)  phi_errlog(core, "tui2", NULL, __VA_ARGS__)
#define syswarnlog(...)  phi_syswarnlog(core, "tui2", NULL, __VA_ARGS__)
#define infolog(...)  phi_infolog(core, "tui2", NULL, __VA_ARGS__)

#define AUTO_LIST_FN  "list%u.m3uz"
#ifdef FF_WIN
	#define USER_HOME  "%APPDATA%"
	#define USER_CONF_DIR  "%APPDATA%\\phiola\\"
#else
	#define USER_HOME  "$HOME"
	#define USER_CONF_DIR  "$HOME/.config/phiola/"
#endif

struct dialog {
	// scroll:
	ushort top;

	// edit:
	u_char len;
	char buf[254];
	u_char numeric;
};

struct tui2_list {
	ushort top, cur;
	uint redrawing :1;
	phi_timer tmr_list_redraw;
	uint counter;
	uint active_track;
	uint save_pending;
	const phi_filter *q_guard;
};

struct tui2_explorer {
	ushort top, cur;
	uint dirs_n;
	char *dir;
	ffdirscanx dx;
};

struct tui2_play_trk;
struct tui2_mod {
	double volume_db;
	ushort list_cap;
	u_char volume;
	u_char popup_type; // enum POPUP
	uint view_explorer :1;
	uint volume_mute :1;
	uint master :1;
	phi_queue_id q_active; // Playlist ID of the currently playing track

	struct tui2_play_trk *playing;
	struct tui2_explorer ex;
	struct tui2_list list;
	struct dialog dlg;

	struct ffncurses_wnd wmain, wpopup;
	char buf[512];

	phi_kevent *kev;
	struct phi_woeh_task task_read;

	// const:
	uint y_status;
	char *user_conf_dir;
	const phi_queue_if *queue;
	phi_task task_init;
};
static struct tui2_mod *mod;
static const phi_core *core;

enum Y {
	Y_TITLE,
	Y_PROGRESS,
	Y_LIST_TITLE,
	Y_LIST,
	// ...
	// y_status
};

enum CLR {
	CLR_TITLE = 1,
	CLR_LIST_SEL,
	CLR_N,
};

enum POPUP {
	POPUP_HELP,
	POPUP_PLAYINFO,
	POPUP_LISTJUMP,
	POPUP_EXPLORERJUMP,
};

#define SEEK_STEP 5
#define SEEK_LEAP 60
#define VOL_STEP 5
#define VOL_MAX  125
#define VOL_LO  (-40)
#define VOL_HI  6

static int explorer_navigate(const char *dir);
static void list_view_title();
static void tui2_exit();

static uint tui2_printf(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	int r = ffs_formatv(mod->buf, sizeof(mod->buf), fmt, va);
	va_end(va);
	if (r < 0)
		r = sizeof(mod->buf) - 1;
	return r;
}

static void tui2_println(struct ffncurses_wnd *w, int y, int x, const char *text, unsigned attr, unsigned color_id)
{
	ffncurses_println_attr(w, y, x, mod->buf, tui2_printf("%s", text), attr, color_id);
}

static void tui2_popup(const char *title, uint scale_pct)
{
	struct ffncurses_rect pos = ffncurses_auto_center(scale_pct);
	ffncurses_popup(&mod->wpopup, pos.h, pos.w, pos.y, pos.x, mod->buf, tui2_printf("%s", title), CLR_TITLE);
	mod->dlg.top = 0;
}

static void tui2_popup_println(uint y, char *text, uint len, unsigned attr, unsigned color_id)
{
	ffncurses_line_clear(&mod->wpopup, y);
	ffncurses_printn_attr(&mod->wpopup, y, 1, text, len, attr, color_id);
}

static void tui2_status(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	int r = ffs_formatv(mod->buf, sizeof(mod->buf), fmt, va);
	va_end(va);
	if (r < 0)
		r = sizeof(mod->buf) - 1;
	ffncurses_println_attr(&mod->wmain, mod->y_status, 0, mod->buf, r, A_BOLD, CLR_TITLE);
	ffncurses_update(&mod->wmain);
}

static void tui2_play_started(struct tui2_play_trk *p, phi_track *t)
{
	mod->playing = p;
	struct phi_queue_entry *qe = (struct phi_queue_entry*)t->qent;
	mod->q_active = mod->queue->queue(qe);
	mod->list.active_track = mod->queue->index(qe);
}

static void tui2_play_finished(const struct tui2_play_trk *p)
{
	if (p != mod->playing) return;

	mod->playing = NULL;
	tui2_println(&mod->wmain, Y_TITLE, 0, "φ", 0, CLR_TITLE);
	ffncurses_line_clear(&mod->wmain, Y_PROGRESS);
	ffncurses_line_clear(&mod->wmain, mod->y_status);
	ffncurses_update(&mod->wmain);
}

static void tui2_dialog_edit_show(const char *title, uint scale, uint numeric)
{
	struct dialog *d = &mod->dlg;
	d->numeric = numeric;
	tui2_popup(title, scale);
	d->len = 0;
	d->buf[d->len++] = '_';
	tui2_popup_println(1, d->buf, d->len, 0, 0);
}

static int tui2_dialog_edit_action(int k)
{
	struct dialog *d = &mod->dlg;
	int key = (k & ~FFKEY_MODMASK);
	uint y = 1;

	switch (k) {
	case FFKEY_ENTER:
	case FFKEY_ESCAPE:
		return k;

	case FFKEY_BACKSPACE:
		if (d->len <= 1)
			return 0;
		d->len--;
		d->buf[d->len - 1] = '_';
		break;

	default:
		if (d->len >= sizeof(d->buf)
			|| (d->numeric && !(key >= '0' && key <= '9'))
			|| (!d->numeric && !(key >= ' ' && key < 0x7f)))
			return 0;

		d->buf[d->len - 1] = key;
		d->buf[d->len++] = '_';
	}

	tui2_popup_println(y, d->buf, d->len, 0, 0);
	return 0;
}

#include <tui2/explorer.h>
#include <tui2/list.h>
#include <tui2/play.h>
#include <tui2/help.h>
#include <tui2/conf.h>

static void list_view_switch();

static void tui2_vol()
{
	if (mod->volume_mute)
		mod->volume_db = -100;
	else if (mod->volume <= 100)
		mod->volume_db = vol2db(mod->volume, VOL_LO);
	else
		mod->volume_db = vol2db_inc(mod->volume - 100, VOL_MAX - 100, VOL_HI);
	tui2_status("Volume: %.02FdB", mod->volume_db);
}

/** Return 0 if handled */
static int play_action(int k, int key)
{
	switch (key) {
	case ' ':
		if (mod->playing)
			tui2_play_pause(mod->playing);
		else
			mod->queue->play(NULL, mod->queue->at(NULL, mod->list.active_track));
		break;

	case 'N':
	case 'P':
		mod->queue->play(mod->q_active, (key == 'N') ? PHI_Q_PLAY_NEXT : PHI_Q_PLAY_PREVIOUS);  break;

	case '.':
		core->track->cmd(NULL, PHI_TRACK_STOP_ALL);  break;

	case 'I':
		play_info_show(mod->playing);  break;

	case 'M':
		mod->volume_mute = !mod->volume_mute;
		tui2_vol();
		tui2_play_volume(mod->playing);
		break;

	case FFKEY_UP:
	case FFKEY_DOWN:
		if ((k & FFKEY_MODMASK) == FFKEY_CTRL) {
			mod->volume += (key == FFKEY_UP) ? VOL_STEP : -VOL_STEP;
			mod->volume = ffmin(mod->volume, VOL_MAX);
			tui2_vol();
			tui2_play_volume(mod->playing);
		} else {
			return 1;
		}
		break;

	case FFKEY_RIGHT:
	case FFKEY_LEFT: {
		int by = ((k & FFKEY_MODMASK) == FFKEY_CTRL) ? SEEK_LEAP : SEEK_STEP;
		if (key == FFKEY_LEFT)
			by = -by;
		tui2_play_seek(mod->playing, by);
		break;
	}

	default:
		return 1;
	}
	return 0;
}

/** Return 0 if handled */
static int global_action(int k, int key)
{
	switch (key) {
	case 'Q':
	case FFKEY_F10:
		if (mod->master && lists_save())
			break;
		tui2_exit();
		break;

	case FFKEY_F1:
		help_show();  break;

	case FFKEY_TAB:
		list_view_switch();  break;

	default:
		return 1;
	}
	return 0;
}

// "Explorer | [Playlist 1] | ..."
static void list_view_title()
{
	uint len = 0, n = mod->queue->total();
	int r;
	const char* const brackets[2][2] = {{"", ""}, {"[", "]"}};

	r = ffs_format(mod->buf + len, sizeof(mod->buf) - len, "%sExplorer%s"
		, brackets[mod->view_explorer][0], brackets[mod->view_explorer][1]);
	len += r;

	phi_queue_id sel = (mod->view_explorer) ? NULL : mod->queue->select(PHI_QSEL_CUR);
	for (uint i = 0;  i < n;  i++) {
		phi_queue_id q = mod->queue->get(i);
		r = ffs_format(mod->buf + len, sizeof(mod->buf) - len, " | %s%s%s"
			, brackets[q == sel][0], mod->queue->conf(q)->name, brackets[q == sel][1]);
		if (r <= 0)
			break; // Reached buffer limit
		len += r;
	}

	ffncurses_println_attr(&mod->wmain, Y_LIST_TITLE, 0, mod->buf, len, A_BOLD, CLR_TITLE);
}

static void list_view_switch()
{
	mod->view_explorer = !mod->view_explorer;
	list_view_title();
	explorer_reset();

	if (mod->view_explorer) {
		explorer_scan(mod->ex.dir);
		explorer_display();
	} else {
		list_display();
		mod->list.redrawing = 0;
	}

	ffncurses_update(&mod->wmain);
}

static void tui2_cmd_read(void *param)
{
	ffstd_ev ev = {};
	ffstr d = {};

	for (;;) {
		if (!d.len) {
			int r = ffstd_keyread(ffstdin, &ev, &d);
			if (r <= 0)
				break;
		}
		// infolog("key sequence: %*xb", d.len, d.ptr);

		int k = ffstd_keyparse(&d);
		if (k == -1) {
			d.len = 0;
			continue;
		}
		int key = k & ~FFKEY_MODMASK;
		if (key >= 'a' && key <= 'z')
			key &= ~0x20; // 'a' -> 'A'

		if (mod->wpopup.wnd) {
			switch (mod->popup_type) {
			case POPUP_PLAYINFO:
				if (!play_info_action(k))
					continue;
				break;

			case POPUP_LISTJUMP:
				if (!list_jump_action(k))
					continue;
				break;

			case POPUP_EXPLORERJUMP:
				if (!explorer_jump_action(k))
					continue;
				break;

			case POPUP_HELP:
				if (!help_action(k))
					continue;
				break;
			}

			mod->popup_type = 0;
			ffncurses_popup_del(&mod->wpopup);
			mod->wmain.modified = 1;
			continue;
		}

		if (!play_action(k, key))
			continue;

		if (!global_action(k, key))
			continue;

		if (mod->view_explorer
			&& !explorer_action(k, key))
			continue;

		list_action(k, key);
	}

	ffncurses_update(&mod->wpopup);
	ffncurses_update(&mod->wmain);

#ifdef FF_WIN
	if (core->woeh(0, ffstdin, &mod->task_read, tui2_cmd_read, NULL, 1)) {
		syswarnlog("establishing stdin event receiver");
	}
#endif
}

static void tui2_init()
{
	mod->volume = 100;
	mod->queue = core->mod("core.queue");
	mod->queue->on_change(q_on_change);
	mod->user_conf_dir = core->conf.env_expand(USER_CONF_DIR);
	if (mod->master)
		conf_load();
	list_init();
	explorer_init();

	// Init ncurses
	struct ffncurses_conf c;
	ffncurses_color(&c, CLR_TITLE, COLOR_MAGENTA, -1);
	ffncurses_color(&c, CLR_LIST_SEL, -1, COLOR_MAGENTA);
	ffncurses_color(&c, CLR_N, -1, -1);

	ffncurses_init(&mod->wmain, &c);
	mod->y_status = ffncurses_height() - 1;
	mod->list_cap = ffncurses_height() - (Y_LIST+1);
	((phi_core*)core)->conf.stdout_busy = 1;

	// Draw UI
	tui2_println(&mod->wmain, Y_TITLE, 0, "φ", 0, CLR_TITLE);
	list_view_title();
	list_display();
	ffncurses_update(&mod->wmain);

	if (mod->master)
		lists_load();

	// Begin reading user commands
#ifdef FF_LINUX
	mod->kev = core->kev_alloc(0);
	mod->kev->rhandler = tui2_cmd_read;
	mod->kev->obj = mod;
	mod->kev->rtask.active = 1;
	if (core->kq_attach(0, mod->kev, ffstdin, 1))
		return;
	if (ffpipe_nonblock(ffstdin, 1))
		syswarnlog("ffpipe_nonblock()");
#endif

	tui2_cmd_read(NULL);
}

static void tui2_exit()
{
	if (mod->master)
		conf_save();
	core->sig(PHI_CORE_STOP);
}

static void tui2_destroy()
{
	ffncurses_end();
	list_close();
	explorer_close();
	ffmem_free(mod->user_conf_dir);
	ffmem_alignfree(mod);
}

static const void* tui2_iface(const char *name)
{
	if (ffsz_eq(name, "play"))
		return &tui2_if_play;
	else if (ffsz_eq(name, "master")) {
		mod->master = 1;
		return (void*)-1;
	}
	return NULL;
}

static const phi_mod tui2_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	tui2_iface, tui2_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_align(sizeof(struct tui2_mod), 4096);
	ffmem_zero_obj(mod);
	core->task(0, &mod->task_init, tui2_init, NULL);
	return &tui2_mod;
}
