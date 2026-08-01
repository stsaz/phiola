/** phiola: TUI-ncurses
2026, Simon Zolin */

#ifdef _WIN32
#include <util/windows-shell.h>
#else
#include <util/unix-shell.h>
#endif
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
#define dbglog(...)  phi_dbglog(core, "tui2", NULL, __VA_ARGS__)

#define AUTO_LIST_FN  "list%u.m3uz"
#ifdef FF_WIN
	#define USER_HOME  "%APPDATA%"
	#define USER_CONF_DIR  "%APPDATA%\\phiola\\"
#else
	#define USER_HOME  "$HOME"
	#define USER_CONF_DIR  "$HOME/.config/phiola/"
#endif

struct rwbuf {
	uint state;
	uint len;
	char *ptr;
	uint w, skipped;
	char chunk[8];
	char buf[512];
};

#define rwbuf_init(b)  (b)->ptr = (b)->buf

static inline void rwbuf_shift(struct rwbuf *b, uint n) {
	b->ptr += n;
	b->len -= n;
}

static inline void rwbuf_reset(struct rwbuf *b) {
	b->ptr = b->buf;
	b->len = b->w = 0;
}

static inline void rwbuf_norm(struct rwbuf *b) {
	memmove(b->buf, b->ptr, b->len);
	b->ptr = b->buf;
	b->w = b->len;
}


struct dialog {
	// scroll:
	ushort top, cur;

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
	u_char volume;
	u_char popup_type; // enum POPUP
	uint bookmark_sec;
	uint view_explorer :1;
	uint volume_mute :1;
	uint master :1;
	uint term_mode_paste :1;
	phi_queue_id q_active; // Playlist ID of the currently playing track
	ffstr pasted_text;

	struct tui2_play_trk *playing;
	struct tui2_explorer ex;
	struct tui2_list list;
	struct dialog dlg;
	struct ffncurses_wnd wmain, wpopup;

	struct rwbuf input FF_STRUCTALIGN(64);
	char buf[512] FF_STRUCTALIGN(64);

	phi_kevent *kev;
	struct phi_woeh_task task_read;

	// const:
	ushort list_cap;
	ushort y_status;
	uint play_info_title :1;
	u_char colors[2];
	char *user_conf_dir;
	const phi_queue_if *queue;
	phi_task task_init;
};
static struct tui2_mod *mod;
static const phi_core *core;

#define X_TITLE 2

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
	POPUP_PLAYINFO,
	POPUP_LIST_JUMP,
	POPUP_LIST_SAVE,
	POPUP_LIST_FRENAME,
	POPUP_LIST_ADDURL,
	POPUP_LIST_SORT,
	POPUP_EXPLORER_JUMP,
	POPUP_HELP,
};

#define SEEK_STEP 5
#define SEEK_LEAP 60
#define VOL_STEP 5
#define VOL_MAX  125
#define VOL_LO  (-40)
#define VOL_HI  6

static void list_view_title();
static void tui2_exit();

/** Copy text into the global buffer. */
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

/** Append a formatted string to the global buffer at offset. */
static uint tui2_appendf(uint off, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	int r = ffs_formatv(mod->buf + off, sizeof(mod->buf) - off, fmt, va);
	va_end(va);
	if (r < 0)
		r = sizeof(mod->buf) - 1;
	else
		r += off;
	return r;
}

/** Print a line inside main window. */
static void tui2_println(struct ffncurses_wnd *w, int y, int x, const char *text, unsigned attr, unsigned color_id)
{
	ffncurses_println_attr(w, y, x, mod->buf, tui2_printf("%s", text), attr, color_id);
}

/** Create a centered popup window with a title. */
static void tui2_popup(const char *title, uint scale_pct)
{
	struct ffncurses_rect pos = ffncurses_auto_center(scale_pct);
	ffncurses_popup(&mod->wpopup, pos.h, pos.w, pos.y, pos.x, mod->buf, tui2_printf("%s", title), CLR_TITLE);
	mod->dlg.top = mod->dlg.cur = 0;
}

/** Print a line inside the dialog. */
static void tui2_popup_println(uint y, char *text, uint len, unsigned attr, unsigned color_id)
{
	ffncurses_line_clear(&mod->wpopup, y);
	ffncurses_printn_attr(&mod->wpopup, y, 1, text, len, attr, color_id);
}

/** Print text on the status bar. */
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

/** Store the currently playing track and its parent playlist. */
static void tui2_play_started(struct tui2_play_trk *p, phi_track *t)
{
	mod->playing = p;
	struct phi_queue_entry *qe = (struct phi_queue_entry*)t->qent;
	mod->q_active = mod->queue->queue(qe);
	mod->list.active_track = mod->queue->index(qe);
}

/** Reset the window title. */
static void title_default()
{
	ffstd_title("φ phiola", 9);
}

/** Clear playback info when a track finishes. */
static void tui2_play_finished(const struct tui2_play_trk *p)
{
	if (p != mod->playing) return;

	mod->playing = NULL;
	ffncurses_line_clear_x(&mod->wmain, Y_TITLE, X_TITLE);
	ffncurses_line_clear(&mod->wmain, Y_PROGRESS);
	ffncurses_line_clear(&mod->wmain, mod->y_status);
	ffncurses_update(&mod->wmain);
	title_default();
}

/** Show an edit dialog in a popup window with initial text. */
static void tui2_dialog_edit_show(const char *title, uint scale, uint numeric, ffstr text)
{
	struct dialog *d = &mod->dlg;
	d->numeric = numeric;
	tui2_popup(title, scale);
	d->len = 0;
	if (text.len)
		d->len = ffmem_ncopy(d->buf, sizeof(d->buf), text.ptr, text.len);
	d->buf[d->len++] = '_';
	tui2_popup_println(1, d->buf, d->len, 0, 0);
	ffstd_paste_ctl(1);
	mod->term_mode_paste = 1;
}

/** Show an edit dialog with a null-terminated string. */
static void tui2_dialog_edit_showz(const char *title, uint scale, uint numeric, const char *text)
{
	tui2_dialog_edit_show(title, scale, numeric, (text) ? FFSTR_Z(text) : FFSTR_Z(""));
}

/** Handle input for the active edit dialog.
Return key code or 0. */
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

	case FFKEY_TEXT_PASTED:
		if (d->len >= sizeof(d->buf))
			return 0;
		d->len--;
		d->len += ffmem_ncopy(d->buf + d->len, sizeof(d->buf) - d->len - 1, mod->pasted_text.ptr, mod->pasted_text.len);
		d->buf[d->len++] = '_';
		break;

	default: {
		if (d->numeric && !(k >= '0' && k <= '9'))
			return 0;

		uint n = ffutf8_len((char*)&k, sizeof(k));
		FF_ASSERT(n);
		if (d->len + n > sizeof(d->buf))
			return 0;
		if (n == 1 && !d->numeric && !(k >= ' ' && k < 0x7f))
			return 0; // non-printable ASCII
		d->len--;
		switch (n) {
		case 4:
		case 3: // cast to `int*` is safe
			*(int*)(d->buf + d->len) = k;  break;

		case 2:
			*(short*)(d->buf + d->len) = k;  break;

		case 1:
			d->buf[d->len] = k;  break;

		default:
			FF_ASSERT(0);
		}
		d->len += n;
		d->buf[d->len++] = '_';
	}
	}

	tui2_popup_println(y, d->buf, d->len, 0, 0);
	return 0;
}

#include <tui2/explorer.h>
#include <tui2/list.h>
#include <tui2/play.h>
#include <tui2/help.h>
#include <tui2/conf.h>

typedef int (*popup_action_t)(int);
static const popup_action_t popup_actions[] = {
	play_info_action,
	list_jump_action,
	list_save_action,
	list_frename_action,
	list_addurl_action,
	list_sort_action,
	explorer_jump_action,
	help_action,
};

/** Toggle between Explorer and Playlist views. */
static void list_view_switch();

/** Update volume and display the level. */
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

	case FFKEY_ALT | 'G':
		if (mod->playing) {
			mod->bookmark_sec = mod->playing->pos_last_sec;
			tui2_status("Bookmark: %u:%02u", mod->bookmark_sec / 60, mod->bookmark_sec % 60);
		}
		break;

	case 'G':
		if (mod->bookmark_sec != ~0U)
			tui2_play_seek(mod->playing, mod->bookmark_sec, 0);
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
		tui2_play_seek(mod->playing, ~0U, by);
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
	struct rwbuf *b = &mod->input;
	int r, k;
	uint n;
	enum { I_READ, I_PARSE, I_PASTED, I_PASTED_SKIP };

	for (;;) {
		switch (b->state) {
		case I_READ:
			FF_ASSERT(b->w < sizeof(b->buf));
			if (0 >= (r = ffstd_key_read(ffstdin, b->buf + b->w, sizeof(b->buf) - b->w)))
				goto end;
			b->w += r;
			b->len += r;
			b->state = I_PARSE;
			// tui2_status("key sequence: %*xb", b->len, b->ptr);
			// fallthrough

		case I_PARSE:
			if (b->len == 1 && b->ptr[0] == '\x1b') {
				rwbuf_shift(b, 1);
				k = FFKEY_ESCAPE;
				break;
			}

			k = ffstd_key_parse(b->ptr, b->len, &n);
			if (k == 0) {
				rwbuf_norm(b);
				b->state = I_READ;
				continue;

			} else if (k == -1) {
				if (n) {
					rwbuf_shift(b, n);
					continue;
				}
				rwbuf_reset(b);
				b->state = I_READ;
				continue;

			} else if (k == FFKEY_TEXT_PASTED) {
				b->state = I_PASTED;
				continue;
			}

			rwbuf_shift(b, n);
			if (k == FFKEY_VIRT)
				continue;
			break;

		case I_PASTED:
			// Extract the text between paste markers
			if (!(n = ffstd_paste_read(b->ptr, b->len, &mod->pasted_text))) {
				if (b->w == sizeof(b->buf)) {
					// Note: 'pasted_text' may contain partial paste-end marker
					dbglog("trimmed pasted text");
					ffmem_copy(b->chunk, b->ptr + b->len - 5, 5); // Preserve tail
					rwbuf_reset(b);
					b->state = I_PASTED_SKIP;
					b->skipped = 0;
					continue;
				}
				rwbuf_norm(b);
				b->state = I_READ;
				continue;
			}

			rwbuf_shift(b, n);
			break;

		case I_PASTED_SKIP: {
			// Too large input: skip until paste-end marker is found
			char tmp[512];
			for (;;) {
				if (b->skipped >= 2*1024*1024) {
					errlog("too large input data");
					return;
				}
				ffmem_copy(tmp, b->chunk, 5);
				if (0 >= (r = ffstd_key_read(ffstdin, tmp + 5, sizeof(tmp) - 5)))
					goto end;
				b->skipped += r;
				n = r + 5;
				if ((r = ffs_findstr(tmp, n, "\e[201~", 6)) >= 0) {
					r += 6;
					break;
				}
				ffmem_copy(b->chunk, tmp + n - 5, 5);
			}
			// Copy unprocessed data to main buffer
			b->len = b->w = n - r;
			ffmem_copy(b->buf, tmp + r, b->w);
			b->state = I_PARSE;
			break;
		}
		}

		int key = k & ~FFKEY_MODMASK;
		if (key >= 'a' && key <= 'z')
			key &= ~0x20; // 'a' -> 'A'

		if (mod->wpopup.wnd) {
			if (!popup_actions[mod->popup_type](k))
				continue;

			if (mod->term_mode_paste) {
				mod->term_mode_paste = 0;
				ffstd_paste_ctl(0);
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

end:
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
	mod->bookmark_sec = ~0U;
	mod->play_info_title = 1;
	mod->colors[0] = COLOR_MAGENTA;
	mod->queue = core->mod("core.queue");
	mod->queue->on_change(q_on_change);
	mod->user_conf_dir = core->conf.env_expand(USER_CONF_DIR);
	if (mod->master)
		conf_load();
	list_init();
	explorer_init();

	// Init ncurses
	struct ffncurses_conf c;
	ffncurses_color(&c, CLR_TITLE, mod->colors[0], -1);
	ffncurses_color(&c, CLR_LIST_SEL, -1, mod->colors[0]);
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

	title_default();

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

	rwbuf_init(&mod->input);
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
