/** GUI-winapi: tray icon
2014, Simon Zolin */

#pragma once
#include "winapi.h"

typedef struct ffui_trayicon {
	NOTIFYICONDATAW nid;
	ffui_menu *pmenu;
	uint lclick_id;
	uint balloon_click_id;
	uint visible :1;
} ffui_trayicon;

FF_EXTERN void ffui_tray_create(ffui_trayicon *t, ffui_wnd *wnd);

#define ffui_tray_visible(t)  ((t)->visible)

/** Set tooltip */
#define ffui_tray_settooltip(t, s, len) \
do { \
	(t)->nid.uFlags |= NIF_TIP; \
	ffsz_utow_n((t)->nid.szTip, FF_COUNT((t)->nid.szTip), s, len); \
} while (0)

#define ffui_tray_settooltipz(t, sz)  ffui_tray_settooltip(t, sz, ffsz_len(sz))

#define ffui_tray_seticon(t, ico) \
do { \
	(t)->nid.uFlags |= NIF_ICON; \
	(t)->nid.hIcon = (ico)->h; \
} while (0)

/** Set balloon tip
flags: NIIF_WARNING NIIF_ERROR NIIF_INFO NIIF_USER NIIF_LARGE_ICON NIIF_NOSOUND */
#define ffui_tray_setinfo(t, title, title_len, text, text_len, flags) \
do { \
	(t)->nid.uFlags |= NIF_INFO; \
	(t)->nid.dwInfoFlags = ((flags) != 0) ? (flags) : NIIF_INFO; \
	ffsz_utow_n((t)->nid.szInfoTitle, FF_COUNT((t)->nid.szInfoTitle), title, title_len); \
	ffsz_utow_n((t)->nid.szInfo, FF_COUNT((t)->nid.szInfo), text, text_len); \
} while (0)

#define ffui_tray_setinfoz(t, title, text, flags) \
do { \
	(t)->nid.uFlags |= NIF_INFO; \
	(t)->nid.dwInfoFlags = ((flags) != 0) ? (flags) : NIIF_INFO; \
	ffsz_utow((t)->nid.szInfoTitle, FF_COUNT((t)->nid.szInfoTitle), title); \
	ffsz_utow((t)->nid.szInfo, FF_COUNT((t)->nid.szInfo), text); \
} while (0)

static inline int ffui_tray_set(ffui_trayicon *t, uint show)
{
	return 0 == Shell_NotifyIconW(NIM_MODIFY, &t->nid);
}

static inline int ffui_tray_show(ffui_trayicon *t, uint show)
{
	uint action = (show) ? NIM_ADD : NIM_DELETE;
	if (show && t->visible)
		action = NIM_MODIFY;
	if (!Shell_NotifyIconW(action, &t->nid))
		return -1;
	t->visible = show;
	return 0;
}
