/** phiola: TUI-ncurses
2026, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <afilter/pcm.h>
#include <util/ffncurses.h>
#include <ffsys/std.h>
#include <ffsys/pipe.h>

#define syswarnlog(t, ...)  phi_syswarnlog(core, "tui2", t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, "tui2", t, __VA_ARGS__)

struct tui2_play_trk;
struct tui2_mod {
	struct tui2_play_trk *playing;
	uint volume;
	uint list_top, list_cur, list_cap;
	struct ffncurses_wnd wmain, wpopup;
	uint popup_help :1;
	uint list_redrawing :1;

	phi_timer tmr_list_redraw;
	phi_kevent *kev;

	uint y_status;
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
};

enum CLR {
	CLR_TITLE = 1,
	CLR_LIST_SEL,
	CLR_N,
};

#define SEEK_STEP 5
#define SEEK_LEAP 60
#define VOL_STEP 5

// "[abcd]ef" -> "a..."
static uint text_clamp(char *s, ffsize len, uint max)
{
	if (len <= max)
		return len;
	if (max > 3)
		s[max - 3] = s[max - 2] = s[max - 1] = '.';
	return max;
}

#include <tui2/play.h>

static void list_display()
{
	ffncurses_println_attr(&mod->wmain, Y_LIST_TITLE, 0, "[Playlist 1]", A_BOLD, CLR_TITLE);

	char buf[500];
	uint w = ffmin(ffncurses_width(), sizeof(buf));
	uint y = Y_LIST_TITLE + 1;
	for (uint i = mod->list_top;  i < mod->list_top + mod->list_cap;  i++) {
		const struct phi_queue_entry *qe = mod->queue->at(NULL, i);
		if (!qe)
			break;

		ffstr s;
		ffpath_split3_str(FFSTR_Z(qe->url), NULL, &s, NULL); // Use file name as title
		int r = ffs_format(buf, sizeof(buf), "%u. %S", i + 1, &s);
		if (r < 0)
			r = -1;
		uint n = text_clamp(buf, r, w);

		ffncurses_line_clear(&mod->wmain, y);
		uint clr = (i == mod->list_cur) ? CLR_LIST_SEL : 0;
		ffncurses_printn_attr(&mod->wmain, y++, 0, buf, n, 0, clr);
	}
}

static void list_redraw_delayed(void *param)
{
	list_display();
	ffncurses_update(&mod->wmain);
	mod->list_redrawing = 0;
}

static void q_on_change(phi_queue_id q, uint flags, uint pos)
{
	switch (flags) {
	case 'a':
		if (!mod->list_redrawing) {
			mod->list_redrawing = 1;
			core->timer(0, &mod->tmr_list_redraw, -50, list_redraw_delayed, NULL);
		}
		break;
	}
}

static void tui2_help()
{
	static const char help_keys[][16] = {
		"Space",			"Pause/Resume",
		"N/P",				"Next/Previous",
		"Left/Right",		"Seek",
		"Ctrl + Up/Down",	"Volume",
		"Up/Down",			"Scroll",
		"Q",				"Quit",
	};
	uint n = FF_COUNT(help_keys)
		, col_width = sizeof(*help_keys);

	uint h = 2 + n / 2
		, w = 4 + col_width * 2
		, y, x;
	ffncurses_center(h, w, &y, &x);
	ffncurses_popup(&mod->wpopup, h, w, y, x, "Help", CLR_TITLE);

	y = 1;
	for (uint i = 0;  i < n;  i += 2) {
		char buf[256];
		uint space = col_width - ffsz_len(help_keys[i]);
		ffsz_format(buf, sizeof(buf), "%s%*c%s"
			, help_keys[i], (ffsize)space, ' '
			, help_keys[i+1]);
		ffncurses_print_attr(&mod->wpopup, y++, 2, buf, 0, 0);
	}

	ffncurses_update(&mod->wpopup);
	mod->popup_help = 1;
}

static void list_scroll(int by, uint abs)
{
	uint cur;
	if (by == 0)
		cur = abs;
	else
		cur = ffmax((int)mod->list_cur + by, 0);
	if (cur >= (uint)mod->queue->count(NULL))
		cur = mod->queue->count(NULL) - 1;

	if (cur < mod->list_top)
		mod->list_top = cur;
	else if (cur >= mod->list_top + mod->list_cap)
		mod->list_top = cur - mod->list_cap + 1;

	mod->list_cur = cur;
	list_display();
}

static void tui2_cmd_read(void *param)
{
	ffstd_ev ev;
	ffstr d = {};

	for (;;) {
		if (!d.len) {
			int r = ffstd_keyread(ffstdin, &ev, &d);
			if (r <= 0)
				break;
		}
		// infolog(NULL, "key sequence: %*xb", d.len, d.ptr);

		int k = ffstd_keyparse(&d);
		if (k == -1) {
			d.len = 0;
			continue;
		}
		int key = k & ~FFKEY_MODMASK;
		if (key >= 'a' && key <= 'z')
			key &= ~0x20;

		if (mod->popup_help) {
			mod->popup_help = 0;
			ffncurses_popup_del(&mod->wpopup);
			mod->wmain.modified = 1;
			continue;
		}

		switch (key) {
		case ' ':
			tui2_play_pause(mod->playing);  break;

		case 'Q':
			core->sig(PHI_CORE_STOP);  break;

		case 'H':
			tui2_help();  break;

		case 'N':
		case 'P':
			mod->queue->play(NULL, (key == 'N') ? PHI_Q_PLAY_NEXT : PHI_Q_PLAY_PREVIOUS);  break;

		case FFKEY_ENTER: {
			struct phi_queue_entry *qe = mod->queue->at(NULL, mod->list_cur);
			mod->queue->play(NULL, qe);
			break;
		}

		case FFKEY_HOME:
		case FFKEY_END:
			list_scroll(0, (key == FFKEY_HOME) ? 0 : ~0U);
			break;

		case FFKEY_PGUP:
		case FFKEY_PGDN:
			list_scroll((key == FFKEY_PGDN) ? mod->list_cap : -(int)mod->list_cap, 0);
			break;

		case FFKEY_UP:
		case FFKEY_DOWN:
			if ((k & FFKEY_MODMASK) == FFKEY_CTRL) {
				mod->volume += (key == FFKEY_UP) ? VOL_STEP : -VOL_STEP;
				mod->volume = ffmin(mod->volume, VOL_MAX);
				tui2_play_volume(mod->playing);

			} else {
				list_scroll((key == FFKEY_DOWN) ? 1 : -1, 0);
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
		}
	}

	ffncurses_update(&mod->wmain);
}

static void tui2_init()
{
	mod->volume = 100;
	mod->queue = core->mod("core.queue");
	mod->queue->on_change(q_on_change);

	struct ffncurses_conf c;
	ffncurses_color(&c, CLR_TITLE, COLOR_MAGENTA, -1);
	ffncurses_color(&c, CLR_LIST_SEL, -1, COLOR_MAGENTA);
	ffncurses_color(&c, CLR_N, -1, -1);

	ffncurses_init(&mod->wmain, &c);
	mod->y_status = ffncurses_height() - 1;
	mod->list_cap = ffncurses_height() - (Y_LIST+1);
	((phi_core*)core)->conf.stdout_busy = 1;
	list_redraw_delayed(NULL);

	mod->kev = core->kev_alloc(0);
	mod->kev->rhandler = tui2_cmd_read;
	mod->kev->obj = mod;
	mod->kev->rtask.active = 1;
	if (core->kq_attach(0, mod->kev, ffstdin, 1))
		return;
	if (ffpipe_nonblock(ffstdin, 1))
		syswarnlog(NULL, "ffpipe_nonblock()");

	tui2_cmd_read(NULL);
}

static void tui2_destroy()
{
	ffncurses_end();
	ffmem_free(mod);
}

static const void* tui2_iface(const char *name)
{
	if (ffsz_eq(name, "play"))
		return &tui2_if_play;
	return NULL;
}

static const phi_mod tui2_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	tui2_iface, tui2_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_new(struct tui2_mod);
	core->task(0, &mod->task_init, tui2_init, NULL);
	return &tui2_mod;
}
