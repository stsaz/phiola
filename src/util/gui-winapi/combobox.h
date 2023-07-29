/** GUI-winapi: combobox
2014, Simon Zolin */

#pragma once
#include "winapi.h"

typedef struct ffui_combx {
	HWND h;
	enum FFUI_UID uid;
	const char *name;
	HFONT font;
	uint change_id;
	uint popup_id;
	uint closeup_id;
	uint edit_change_id;
	uint edit_update_id;
} ffui_combx;

FF_EXTERN int ffui_combx_create(ffui_ctl *c, ffui_wnd *parent);

/** Insert an item
idx: -1: insert to end */
static inline void ffui_combx_ins(ffui_combx *c, int idx, const char *txt, ffsize len)
{
	ffsyschar *w, ws[255];
	ffsize n = FF_COUNT(ws) - 1;
	if (NULL == (w = ffs_utow(ws, &n, txt, len)))
		return;
	w[n] = '\0';
	uint msg = CB_INSERTSTRING;
	if (idx == -1) {
		idx = 0;
		msg = CB_ADDSTRING;
	}
	ffui_ctl_send(c, msg, idx, w);
	if (w != ws)
		ffmem_free(w);
}
#define ffui_combx_insz(c, idx, textz)  ffui_combx_ins(c, idx, textz, ffsz_len(textz))

/** Remove item */
#define ffui_combx_rm(c, idx)  ffui_ctl_send(c, CB_DELETESTRING, idx, 0)

/** Remove all items */
#define ffui_combx_clear(c)  ffui_ctl_send(c, CB_RESETCONTENT, 0, 0)

/** Get number of items */
#define ffui_combx_count(c)  ((uint)ffui_ctl_send(c, CB_GETCOUNT, 0, 0))

/** Set/get active index */
#define ffui_combx_set(c, idx)  ffui_ctl_send(c, CB_SETCURSEL, idx, 0)
#define ffui_combx_active(c)  ((uint)ffui_ctl_send(c, CB_GETCURSEL, 0, 0))

/** Get text */
FF_EXTERN int ffui_combx_textstr(ffui_combx *c, uint idx, ffstr *dst);

/** Show/hide drop down list */
#define ffui_combx_popup(c, show)  ffui_ctl_send(c, CB_SHOWDROPDOWN, show, 0)
