/** GUI-winapi: window
2014, Simon Zolin */

#pragma once
#include "winapi.h"

struct ffui_wnd {
	HWND h;
	enum FFUI_UID uid;
	const char *name;
	HFONT font;
	HBRUSH bgcolor;
	uint top :1 //quit message loop if the window is closed
		, hide_on_close :1 //window doesn't get destroyed when it's closed
		, manual_close :1 //don't automatically close window on X button press
		, popup :1;
	byte bordstick; //stick window to screen borders

	HWND ttip;
	HWND focused; //restore focus when the window is activated again
	ffui_trayicon *trayicon;
	ffui_paned *paned_first;
	ffui_stbar *stbar;
	HACCEL acceltbl;
	ffvec ghotkeys; //struct wnd_ghotkey[]

	void (*on_create)(ffui_wnd *wnd);
	void (*on_destroy)(ffui_wnd *wnd);
	void (*on_action)(ffui_wnd *wnd, int id);
	void (*on_dropfiles)(ffui_wnd *wnd, ffui_fdrop *df);

	/** WM_PAINT handler. */
	void (*on_paint)(ffui_wnd *wnd);

	uint onclose_id;
	uint onminimize_id;
	uint onmaximize_id;
	uint onactivate_id;
};

FF_EXTERN int ffui_wnd_initstyle(void);

static inline void ffui_wnd_setpopup(ffui_wnd *w)
{
	LONG st = GetWindowLongW(w->h, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW;
	SetWindowLongW(w->h, GWL_STYLE, st | WS_POPUPWINDOW | WS_CAPTION | WS_THICKFRAME);
	w->popup = 1;
}

enum {
	FFUI_WM_USER_TRAY = WM_USER + 1000
};

FF_EXTERN int ffui_wndproc(ffui_wnd *wnd, ffsize *code, HWND h, uint msg, ffsize w, ffsize l);

FF_EXTERN int ffui_wnd_create(ffui_wnd *w);

#define ffui_desktop(w)  (w)->h = GetDesktopWindow()

#define ffui_wnd_front(w)  (w)->h = GetForegroundWindow()
#define ffui_wnd_setfront(w)  SetForegroundWindow((w)->h)

static inline void ffui_wnd_icon(ffui_wnd *w, ffui_icon *big_ico, ffui_icon *small_ico)
{
	big_ico->h = (HICON)ffui_ctl_send(w, WM_GETICON, ICON_BIG, 0);
	small_ico->h = (HICON)ffui_ctl_send(w, WM_GETICON, ICON_SMALL, 0);
}

static inline void ffui_wnd_seticon(ffui_wnd *w, const ffui_icon *big_ico, const ffui_icon *small_ico)
{
	ffui_ctl_send(w, WM_SETICON, ICON_SMALL, small_ico->h);
	ffui_ctl_send(w, WM_SETICON, ICON_BIG, big_ico->h);
}

/** Set background color. */
static inline void ffui_wnd_bgcolor(ffui_wnd *w, uint color)
{
	if (w->bgcolor != NULL)
		DeleteObject(w->bgcolor);
	w->bgcolor = CreateSolidBrush(color);
}

static inline void ffui_wnd_opacity(ffui_wnd *w, uint percent)
{
	LONG_PTR L = GetWindowLongPtrW(w->h, GWL_EXSTYLE);

	if (percent >= 100) {
		SetWindowLongPtrW(w->h, GWL_EXSTYLE, L & ~WS_EX_LAYERED);
		return;
	}

	if (!(L & WS_EX_LAYERED))
		SetWindowLongPtrW(w->h, GWL_EXSTYLE, L | WS_EX_LAYERED);

	SetLayeredWindowAttributes(w->h, 0, 255 * percent / 100, LWA_ALPHA);
}

#define ffui_wnd_close(w)  ffui_ctl_send(w, WM_CLOSE, 0, 0)

FF_EXTERN int ffui_wnd_destroy(ffui_wnd *w);

#define ffui_wnd_pos(w, pos) \
	ffui_getpos2(w, pos, FFUI_FPOS_DPISCALE)

/** Get window placement.
Return SW_*. */
static inline uint ffui_wnd_placement(ffui_wnd *w, ffui_pos *pos)
{
	WINDOWPLACEMENT pl = {};
	pl.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(w->h, &pl);
	ffui_pos_fromrect(pos, &pl.rcNormalPosition);
	if (pl.showCmd == SW_SHOWNORMAL && !IsWindowVisible(w->h))
		pl.showCmd = SW_HIDE;
	return pl.showCmd;
}

static inline void ffui_wnd_setplacement(ffui_wnd *w, uint showcmd, const ffui_pos *pos)
{
	WINDOWPLACEMENT pl = {};
	pl.length = sizeof(WINDOWPLACEMENT);
	pl.showCmd = showcmd;
	ffui_pos_torect(pos, &pl.rcNormalPosition);
	SetWindowPlacement(w->h, &pl);
}

static inline int ffui_wnd_tooltip(ffui_wnd *w, ffui_ctl *ctl, const char *text, ffsize len)
{
	TTTOOLINFOW ti = {};
	ffsyschar *pw, ws[255];
	ffsize n = FF_COUNT(ws) - 1;

	if (w->ttip == NULL
		&& NULL == (w->ttip = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL
			, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP
			, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT
			, NULL, NULL, NULL, NULL)))
		return -1;

	if (NULL == (pw = ffs_utow(ws, &n, text, len)))
		return -1;
	pw[n] = '\0';

	ti.cbSize = sizeof(ti);
	ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
	ti.hwnd = ctl->h;
	ti.uId = (UINT_PTR)ctl->h;
	ti.lpszText = pw;
	ffui_send(w->ttip, TTM_ADDTOOL, 0, &ti);

	if (pw != ws)
		ffmem_free(pw);
	return 0;
}

typedef struct ffui_wnd_hotkey {
	uint hk;
	uint cmd;
} ffui_wnd_hotkey;

/** Set hotkey table. */
FF_EXTERN int ffui_wnd_hotkeys(ffui_wnd *w, const struct ffui_wnd_hotkey *hotkeys, ffsize n);

/** Register a global hotkey. */
FF_EXTERN int ffui_wnd_ghotkey_reg(ffui_wnd *w, uint hk, uint cmd);

/** Unregister all global hotkeys associated with this window. */
FF_EXTERN void ffui_wnd_ghotkey_unreg(ffui_wnd *w);


#ifdef __cplusplus
struct ffui_wndxx : ffui_wnd {
	void show(uint show) { ffui_show(this, show); }
	void title(const char *sz) { ffui_settextz(this, sz); }
	void close() { ffui_wnd_close(this); }
	ffui_pos pos() { ffui_pos p; ffui_wnd_placement(this, &p); return p; }
	void place(const ffui_pos &pos) { ffui_wnd_setplacement(this, SW_SHOWNORMAL, &pos); }
};
#endif
