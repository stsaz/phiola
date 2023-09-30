/** GUI/GTK+: window
2019,2022, Simon Zolin
*/

#pragma once
#include "gtk.h"

struct ffui_wnd {
	GtkWindow *h;
	enum FFUI_UID uid;
	GtkWidget *vbox;

	void (*on_create)(ffui_wnd *wnd);
	void (*on_destroy)(ffui_wnd *wnd);
	void (*on_action)(ffui_wnd *wnd, int id);

	uint onclose_id;
	uint hide_on_close :1;
};

static inline int ffui_wnd_initstyle()
{
	return 0;
}

FF_EXTERN int ffui_wnd_create(ffui_wnd *w);

#define ffui_wnd_close(w)  gtk_window_close((w)->h)

#define ffui_wnd_destroy(w)  gtk_widget_destroy(GTK_WIDGET((w)->h))

FF_EXTERN void ffui_wnd_setpopup(ffui_wnd *w, ffui_wnd *parent);

#define ffui_wnd_present(w)  gtk_window_present((w)->h)

static inline void ffui_wnd_setmenu(ffui_wnd *w, ffui_menu *m)
{
	m->wnd = w;
	gtk_box_pack_start(GTK_BOX(w->vbox), m->h, /*expand=*/0, /*fill=*/0, /*padding=*/0);
}

typedef uint ffui_hotkey;

#define ffui_hotkey_mod(hk)  ((hk) >> 16)
#define ffui_hotkey_key(hk)  ((hk) & 0xffff)

/** Parse hotkey string, e.g. "Ctrl+Alt+Shift+Q".
Return: low-word: char key or vkey, hi-word: control flags;  0 on error. */
FF_EXTERN ffui_hotkey ffui_hotkey_parse(const char *s, ffsize len);

typedef struct ffui_wnd_hotkey {
	ffui_hotkey hk;
	GtkWidget *h;
} ffui_wnd_hotkey;

/** Set hotkey table. */
FF_EXTERN int ffui_wnd_hotkeys(ffui_wnd *w, const ffui_wnd_hotkey *hotkeys, ffsize n);

#define ffui_wnd_settextz(w, text)  gtk_window_set_title((w)->h, text)
static inline void ffui_wnd_settextstr(ffui_wnd *w, const ffstr *str)
{
	char *sz = ffsz_dupstr(str);
	ffui_wnd_settextz(w, sz);
	ffmem_free(sz);
}

#define ffui_wnd_seticon(w, icon)  gtk_window_set_icon((w)->h, (icon)->ico)

static inline void ffui_wnd_pos(ffui_wnd *w, ffui_pos *pos)
{
	int x, y, ww, h;
	gtk_window_get_position(w->h, &x, &y);
	gtk_window_get_size(w->h, &ww, &h);
	pos->x = x;
	pos->y = y;
	pos->cx = ww;
	pos->cy = h;
}
#define ffui_wnd_placement(w, pos)  ({ ffui_wnd_pos(w, pos); 0; })

#define SW_SHOWNORMAL 0

static inline void ffui_wnd_setplacement(ffui_wnd *w, uint showcmd, const ffui_pos *pos)
{
	gtk_window_move(w->h, pos->x, pos->y);
	gtk_window_set_default_size(w->h, pos->cx, pos->cy);
}
