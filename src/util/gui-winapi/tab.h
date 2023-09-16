/** GUI-winapi: tab
2014, Simon Zolin */

#pragma once
#include "winapi.h"

typedef struct ffui_tab {
	HWND h;
	enum FFUI_UID uid;
	const char *name;
	HFONT font;
	int changing_sel_id;
	uint changing_sel_keep :1; // User sets it to prevent the tab from changing
	int chsel_id;
} ffui_tab;

FF_EXTERN int ffui_tab_create(ffui_tab *t, ffui_wnd *parent);

#define ffui_tab_active(t)  ffui_ctl_send(t, TCM_GETCURSEL, 0, 0)
#define ffui_tab_setactive(t, idx)  ffui_ctl_send(t, TCM_SETCURSEL, idx, 0)

#define ffui_tab_count(t)  ffui_ctl_send(t, TCM_GETITEMCOUNT, 0, 0)

#define ffui_tab_del(t, idx)  ffui_ctl_send(t, TCM_DELETEITEM, idx, 0)
#define ffui_tab_clear(t)  ffui_ctl_send(t, TCM_DELETEALLITEMS, 0, 0)

typedef struct ffui_tabitem {
	TCITEMW item;
	wchar_t wtext[255];
	wchar_t *w;
} ffui_tabitem;

static inline void ffui_tab_reset(ffui_tabitem *it)
{
	if (it->w != it->wtext)
		ffmem_free(it->w);
	ffmem_zero_obj(it);
}

#define ffui_tab_settext_q(it, txt) \
do { \
	(it)->item.mask |= TCIF_TEXT; \
	(it)->item.pszText = txt; \
} while (0)

#define ffui_tab_gettext(it) \
do { \
	(it)->item.mask |= TCIF_TEXT; \
	(it)->item.pszText = (it)->wtext; \
	(it)->item.cchTextMax = FF_COUNT((it)->wtext); \
} while (0)

static inline void ffui_tab_settext(ffui_tabitem *it, const char *txt, ffsize len)
{
	ffsize n = FF_COUNT(it->wtext) - 1;
	if (NULL == (it->w = ffs_utow(it->wtext, &n, txt, len)))
		return;
	it->w[n] = '\0';
	ffui_tab_settext_q(it, it->w);
}
#define ffui_tab_settextz(it, sz)  ffui_tab_settext(it, sz, ffsz_len(sz))

/**
iconlist_idx: image list index */
#define ffui_tab_seticon(it, iconlist_idx) \
do { \
	(it)->item.mask |= TCIF_IMAGE; \
	(it)->item.iImage = (iconlist_idx); \
} while (0)

static inline void ffui_tab_seticonlist(ffui_tab *t, ffui_iconlist *il)
{
	TabCtrl_SetImageList(t->h, il->h);
}

static inline int ffui_tab_ins(ffui_tab *t, int idx, ffui_tabitem *it)
{
	int r = ffui_ctl_send(t, TCM_INSERTITEMW, idx, &it->item);
	ffui_tab_reset(it);
	return r;
}

#define ffui_tab_append(t, it)  ffui_tab_ins(t, ffui_tab_count(t), it)

static inline ffbool ffui_tab_set(ffui_tab *t, int idx, ffui_tabitem *it)
{
	int r = ffui_ctl_send(t, TCM_SETITEMW, idx, &it->item);
	ffui_tab_reset(it);
	return r;
}

static inline ffbool ffui_tab_get(ffui_tab *t, int idx, ffui_tabitem *it)
{
	int r = ffui_ctl_send(t, TCM_GETITEMW, idx, &it->item);
	ffui_tab_reset(it);
	return r;
}


#ifdef __cplusplus
struct ffui_tabxx : ffui_tab {
	void add(const char *sz) {
		ffui_tabitem ti = {};
		ffui_tab_settextz(&ti, sz);
		ffui_tab_append(this, &ti);
	}
	void del(u_int i) { ffui_tab_del(this, i); }
	void select(u_int i) { ffui_tab_setactive(this, i); }
	u_int count() { return ffui_tab_count(this); }
	u_int changed() { return ffui_tab_active(this); }
};
#endif
