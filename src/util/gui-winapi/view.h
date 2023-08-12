/** GUI-winapi: listview
2014, Simon Zolin */

#pragma once
#include "winapi.h"

typedef struct ffui_view {
	HWND h;
	enum FFUI_UID uid;
	const char *name;
	HFONT font;
	ffui_menu *pmenu;
	int chsel_id;
	int lclick_id;
	int dblclick_id;
	int colclick_id; //"col" is set to column #
	int edit_id; // "text" contains the text edited by user
	int check_id; //checkbox has been (un)checked.  "idx" is the item index.

	/** Owner-data callback.  User must fill in "dispinfo_item" object.
	Must be set before ffui_view_create(). */
	int dispinfo_id;

	union {
	int idx;
	int col;
	char *text;
	LVITEMW *dispinfo_item;
	};
} ffui_view;

FF_EXTERN int ffui_view_create(ffui_view *c, ffui_wnd *parent);

#define ffui_view_settheme(v)  SetWindowTheme((v)->h, L"Explorer", NULL)

/** Get index of the item by screen coordinates.
subitem: optional
flags: LVHT_*
Return item index or -1 on error. */
static inline int ffui_view_hittest2(ffui_view *v, const ffui_point *pt, int *subitem, uint *flags)
{
	LVHITTESTINFO ht = {};
	ffui_point cpt = *pt;
	ffui_screen2client(v, &cpt);
	ht.pt.x = cpt.x;
	ht.pt.y = cpt.y;
	ht.flags = *flags;
	ht.iItem = -1;
	ht.iSubItem = -1;
#if FF_WIN >= 0x0600
	ht.iGroup = -1;
#endif
	if (0 > (int)ffui_send(v->h, LVM_SUBITEMHITTEST, -1, &ht))
		return -1;

	if (!(ht.flags & *flags))
		return -1;

	if (subitem != NULL)
		*subitem = ht.iSubItem;
	*flags = ht.flags;
	return ht.iItem;
}

static inline int ffui_view_hittest(ffui_view *v, const ffui_point *pt, int *subitem)
{
	uint f = LVHT_ONITEM;
	return ffui_view_hittest2(v, pt, subitem, &f);
}

/** Get top visible item index. */
#define ffui_view_topindex(v)  ffui_ctl_send(v, LVM_GETTOPINDEX, 0, 0)

#define ffui_view_makevisible(v, idx)  ffui_ctl_send(v, LVM_ENSUREVISIBLE, idx, /*partial_ok*/ 0)
#define ffui_view_scroll(v, dx, dy)  ffui_ctl_send(v, LVM_SCROLL, dx, dy)

FF_EXTERN int ffui_view_itempos(ffui_view *v, uint idx, ffui_pos *pos);


// LISTVIEW COLUMN
/** Get the number of columns. */
#define ffui_view_ncols(v) \
	ffui_send((HWND)ffui_ctl_send(v, LVM_GETHEADER, 0, 0), HDM_GETITEMCOUNT, 0, 0)

typedef struct ffui_viewcol {
	LVCOLUMNW col;
	wchar_t text[255];
} ffui_viewcol;

static inline void ffui_viewcol_reset(ffui_viewcol *vc)
{
	ffmem_zero_obj(&vc->col);
	vc->text[0] = '\0';
}

#define ffui_viewcol_settext_q(vc, sz) \
do { \
	(vc)->col.mask |= LVCF_TEXT; \
	(vc)->col.pszText = (sz); \
} while (0)

static inline void ffui_viewcol_settext(ffui_viewcol *vc, const char *text, ffsize len)
{
	ffsize n;
	if (0 == (n = ff_utow(vc->text, FF_COUNT(vc->text) - 1, text, len, 0))
		&& len != 0)
		return;
	vc->text[n] = '\0';
	ffui_viewcol_settext_q(vc, vc->text);
}

static inline uint ffui_viewcol_width(ffui_viewcol *vc)
{
	return _ffui_dpi_descale(vc->col.cx);
}

static inline void ffui_viewcol_setwidth(ffui_viewcol *vc, uint w)
{
	vc->col.mask |= LVCF_WIDTH;
	vc->col.cx = _ffui_dpi_scale(w);
}

/**
'a': HDF_LEFT HDF_RIGHT HDF_CENTER */
#define ffui_viewcol_setalign(vc, a) \
do { \
	(vc)->col.mask |= LVCF_FMT; \
	(vc)->col.fmt |= (a); \
} while (0)

/**
'f': HDF_SORTUP HDF_SORTDOWN */
#define ffui_viewcol_setsort(vc, f) \
do { \
	(vc)->col.mask |= LVCF_FMT; \
	(vc)->col.fmt |= (f); \
} while (0)

#define ffui_viewcol_sort(vc)  ((vc)->col.fmt & (HDF_SORTUP | HDF_SORTDOWN))

#define ffui_viewcol_setorder(vc, ord) \
do { \
	(vc)->col.mask |= LVCF_ORDER; \
	(vc)->col.iOrder = (ord); \
} while (0)

static inline void ffui_view_inscol(ffui_view *v, int pos, ffui_viewcol *vc)
{
	ffui_ctl_send(v, LVM_INSERTCOLUMNW, pos, vc);
	ffui_viewcol_reset(vc);
}

#define ffui_view_delcol(v, i)  ListView_DeleteColumn((v)->h, i)

static inline void ffui_view_setcol(ffui_view *v, int i, ffui_viewcol *vc)
{
	ffui_ctl_send(v, LVM_SETCOLUMNW, i, &vc->col);
	ffui_viewcol_reset(vc);
}

static inline void ffui_view_col(ffui_view *v, int i, ffui_viewcol *vc)
{
	ffui_ctl_send(v, LVM_GETCOLUMNW, i, &vc->col);
}


// LISTVIEW GROUP
typedef struct ffui_viewgrp {
	LVGROUP grp;
	wchar_t text[255];
} ffui_viewgrp;

static inline void ffui_viewgrp_reset(ffui_viewgrp *vg)
{
	vg->grp.cbSize = sizeof(vg->grp);
	vg->grp.mask = LVGF_GROUPID;
}

#define ffui_viewgrp_settext_q(vg, sz) \
do { \
	(vg)->grp.mask |= LVGF_HEADER; \
	(vg)->grp.pszHeader = (sz); \
} while (0)

#if (FF_WIN >= 0x0600)
#define ffui_viewgrp_setsubtitle_q(vg, sz) \
do { \
	(vg)->grp.mask |= LVGF_SUBTITLE; \
	(vg)->grp.pszSubtitle = (sz); \
} while (0)
#endif

static inline void ffui_viewgrp_settext(ffui_viewgrp *vg, const char *text, ffsize len)
{
	ffsize n;
	if (0 == (n = ff_utow(vg->text, FF_COUNT(vg->text) - 1, text, len, 0))
		&& len != 0)
		return;
	vg->text[n] = '\0';
	ffui_viewgrp_settext_q(vg, vg->text);
}
#define ffui_viewgrp_settextz(c, sz)  ffui_viewgrp_settext(c, sz, ffsz_len(sz))
#define ffui_viewgrp_settextstr(c, str)  ffui_viewgrp_settext(c, (str)->ptr, (str)->len)

#define ffui_viewgrp_setcollapsible(vg, val) \
do { \
	(vg)->grp.mask |= LVGF_STATE; \
	(vg)->grp.stateMask |= LVGS_COLLAPSIBLE; \
	(vg)->grp.state |= (val) ? LVGS_COLLAPSIBLE : 0; \
} while (0)


#define ffui_view_cleargroups(v)  ffui_send((v)->h, LVM_REMOVEALLGROUPS, 0, 0)

#define ffui_view_showgroups(v, show)  ffui_send((v)->h, LVM_ENABLEGROUPVIEW, show, 0)

#if (FF_WIN >= 0x0600)
#define ffui_view_ngroups(v)  ffui_send((v)->h, LVM_GETGROUPCOUNT, 0, 0)
#endif

#define ffui_view_delgrp(v, i)  ffui_send((v)->h, LVM_REMOVEGROUP, i, 0)

static inline int ffui_view_insgrp(ffui_view *v, int pos, int id, ffui_viewgrp *vg)
{
	vg->grp.iGroupId = id;
	int r = ffui_send(v->h, LVM_INSERTGROUP, pos, &vg->grp);
	ffui_viewgrp_reset(vg);
	return r;
}

static inline void ffui_view_setgrp(ffui_view *v, int i, ffui_viewgrp *vg)
{
	ffui_send(v->h, LVM_SETGROUPINFO, i, &vg->grp);
	ffui_viewgrp_reset(vg);
}

static inline void ffui_view_grp(ffui_view *v, int i, ffui_viewgrp *vg)
{
	ffui_send(v->h, LVM_GETGROUPINFO, i, &vg->grp);
}


#define ffui_view_nitems(v)  ListView_GetItemCount((v)->h)
#define ffui_view_setcount(v, n) \
	ffui_ctl_send(v, LVM_SETITEMCOUNT, n, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL)
#define ffui_view_setcount_redraw(v, n) \
	ffui_ctl_send(v, LVM_SETITEMCOUNT, n, 0)

/** Redraw items in range. */
#define ffui_view_redraw(v, first, last) \
	ffui_ctl_send(v, LVM_REDRAWITEMS, first, last)

typedef struct ffui_viewitem {
	LVITEMW item;
	wchar_t wtext[255];
	wchar_t *w;
} ffui_viewitem;

static inline void ffui_view_iteminit(ffui_viewitem *it)
{
	it->item.mask = 0;
	it->w = NULL;
}

static inline void ffui_view_itemreset(ffui_viewitem *it)
{
	it->item.mask = 0;
	it->item.stateMask = 0;
	it->item.state = 0;
	if (it->w != it->wtext && it->w != NULL) {
		ffmem_free(it->w);
		it->w = NULL;
	}
}

#define ffui_view_setindex(it, idx)  (it)->item.iItem = (idx)

enum {
	_FFUI_VIEW_UNCHECKED = 0x1000,
	_FFUI_VIEW_CHECKED = 0x2000,
};

#define ffui_view_check(it, checked) \
do { \
	(it)->item.mask |= LVIF_STATE; \
	(it)->item.stateMask |= LVIS_STATEIMAGEMASK; \
	(it)->item.state &= ~LVIS_STATEIMAGEMASK; \
	(it)->item.state |= (checked) ? _FFUI_VIEW_CHECKED : _FFUI_VIEW_UNCHECKED; \
} while (0)

#define ffui_view_checked(it)  (((it)->item.state & LVIS_STATEIMAGEMASK) == _FFUI_VIEW_CHECKED)

#define ffui_view_focus(it, focus) \
do { \
	(it)->item.mask |= LVIF_STATE; \
	(it)->item.stateMask |= LVIS_FOCUSED; \
	(it)->item.state |= (focus) ? LVIS_FOCUSED : 0; \
} while (0)

#define ffui_view_select(it, select) \
do { \
	(it)->item.mask |= LVIF_STATE; \
	(it)->item.stateMask |= LVIS_SELECTED; \
	if (select) \
		(it)->item.state |= LVIS_SELECTED; \
	else \
		(it)->item.state &= ~LVIS_SELECTED; \
} while (0)

#define ffui_view_selected(it)  !!((it)->item.state & LVIS_SELECTED)

#define ffui_view_setgroupid(it, grp) \
do { \
	(it)->item.mask |= LVIF_GROUPID; \
	(it)->item.iGroupId = grp; \
} while (0)

#define ffui_view_groupid(it)  ((it)->item.iGroupId)

/** Set user data for an item.
Note: insertion is very slow for lists with >300 items. */
#define ffui_view_setparam(it, param) \
do { \
	(it)->item.mask |= LVIF_PARAM; \
	(it)->item.lParam = (LPARAM)(param); \
} while (0)

#define ffui_view_setimg(it, img_idx) \
do { \
	(it)->item.mask |= LVIF_IMAGE; \
	(it)->item.iImage = (img_idx); \
} while (0)

#define ffui_view_gettext(it) \
do { \
	(it)->item.mask |= LVIF_TEXT; \
	(it)->item.pszText = (it)->wtext; \
	(it)->item.cchTextMax = FF_COUNT((it)->wtext); \
} while (0)

#define ffui_view_textq(it)  ((it)->item.pszText)

#define ffui_view_settext_q(it, sz) \
do { \
	(it)->item.mask |= LVIF_TEXT; \
	(it)->item.pszText = (wchar_t*)(sz); \
} while (0)

static inline void ffui_view_settext(ffui_viewitem *it, const char *text, ffsize len)
{
	ffsize n = FF_COUNT(it->wtext) - 1;
	if (NULL == (it->w = ffs_utow(it->wtext, &n, text, len)))
		return;
	it->w[n] = '\0';
	ffui_view_settext_q(it, it->w);
}
#define ffui_view_settextz(it, sz)  ffui_view_settext(it, sz, ffsz_len(sz))
#define ffui_view_settextstr(it, str)  ffui_view_settext(it, (str)->ptr, (str)->len)

static inline int ffui_view_ins(ffui_view *v, int pos, ffui_viewitem *it)
{
	it->item.iItem = pos;
	it->item.iSubItem = 0;
	pos = ffui_ctl_send(v, LVM_INSERTITEMW, 0, it);
	ffui_view_itemreset(it);
	return pos;
}

#define ffui_view_append(v, it)  ffui_view_ins(v, ffui_view_nitems(v), it)

static inline int ffui_view_set(ffui_view *v, int sub, ffui_viewitem *it)
{
	uint check_id = v->check_id, chsel_id = v->chsel_id;
	v->check_id = 0;  v->chsel_id = 0;
	int r;
	it->item.iSubItem = sub;
	r = (0 == ffui_ctl_send(v, LVM_SETITEMW, 0, it));
	ffui_view_itemreset(it);
	v->check_id = check_id;
	v->chsel_id = chsel_id;
	return r;
}

/*
Getting large text: call ListView_GetItem() with a larger buffer until the whole data has been received. */
static inline int ffui_view_get(ffui_view *v, int sub, ffui_viewitem *it)
{
	size_t cap = 4096;
	it->item.iSubItem = sub;

	while (ffui_ctl_send(v, LVM_GETITEMW, 0, &it->item)) {

		if (!(it->item.mask & LVIF_TEXT)
			|| wcslen(it->item.pszText) + 1 != (size_t)it->item.cchTextMax)
			return 0;

		wchar_t *p;
		if (NULL == (p = (wchar_t*)ffmem_realloc(it->w, cap * sizeof(wchar_t))))
			return -1;
		it->w = p;
		it->item.pszText = it->w;
		it->item.cchTextMax = cap;
		cap *= 2;
	}

	return -1;
}

#define ffui_view_param(it)  ((it)->item.lParam)

/** Find the first item associated with user data (ffui_view_param()). */
FF_EXTERN int ffui_view_search(ffui_view *v, ffsize by);

#define ffui_view_focused(v)  (int)ffui_ctl_send(v, LVM_GETNEXTITEM, -1, LVNI_FOCUSED)

#define ffui_view_clear(v)  ListView_DeleteAllItems((v)->h)

#define ffui_view_rm(v, item)  ListView_DeleteItem((v)->h, item)

#define ffui_view_selcount(v)  ffui_ctl_send(v, LVM_GETSELECTEDCOUNT, 0, 0)
#define ffui_view_selnext(v, from)  ffui_ctl_send(v, LVM_GETNEXTITEM, from, LVNI_SELECTED)
#define ffui_view_sel(v, i)  ListView_SetItemState((v)->h, i, LVIS_SELECTED, LVIS_SELECTED)
#define ffui_view_unsel(v, i)  ListView_SetItemState((v)->h, i, 0, LVIS_SELECTED)
#define ffui_view_selall(v)  ffui_view_sel(v, -1)
#define ffui_view_unselall(v)  ffui_view_unsel(v, -1)
FF_EXTERN int ffui_view_sel_invert(ffui_view *v);

/** Sort items by lParam (set by ffui_view_setparam()).
'func': int __stdcall sort(LPARAM p1, LPARAM p2, LPARAM udata) */
#define ffui_view_sort(v, func, udata)  ListView_SortItems((v)->h, func, udata)

#define ffui_view_clr_text(v, val)  ffui_send((v)->h, LVM_SETTEXTCOLOR, 0, val)
#define ffui_view_clr_bg(v, val) \
do { \
	ffui_send((v)->h, LVM_SETBKCOLOR, 0, val); \
	ffui_send((v)->h, LVM_SETTEXTBKCOLOR, 0, val); \
} while (0)

#define _ffui_view_edit(v, i)  ((HWND)ffui_send((v)->h, LVM_EDITLABEL, i, 0))

/** Show edit box with an item's text.
Listview must have LVS_EDITLABELS.
When editing is finished, 'ffui_view.edit_id' is sent. */
static inline HWND ffui_view_edit(ffui_view *v, uint i, uint sub)
{
	ffui_viewitem it;

	ffui_view_iteminit(&it);
	ffui_view_setindex(&it, i);
	ffui_view_gettext(&it);
	ffui_view_get(v, sub, &it);

	ffui_edit e;
	e.h = _ffui_view_edit(v, i);
	ffui_send(e.h, WM_SETTEXT, NULL, ffui_view_textq(&it));
	ffui_view_itemreset(&it);
	ffui_edit_selall(&e);
	return e.h;
}

/** Start editing a sub-item if it's under mouse cursor.
Return item index. */
FF_EXTERN int ffui_view_edit_hittest(ffui_view *v, uint sub);

/** Set sub-item's text after the editing is done. */
FF_EXTERN void ffui_view_edit_set(ffui_view *v, uint i, uint sub);

#define ffui_view_seticonlist(v, il)  ListView_SetImageList((v)->h, (il)->h, LVSIL_SMALL)

typedef LVITEMW ffui_view_disp;

/** Set text on a 'dispinfo_item' object. */
static inline void ffui_view_dispinfo_settext(LVITEMW *it, const char *text, ffsize len)
{
	if (!(it->mask & LVIF_TEXT) || it->cchTextMax == 0)
		return;
	uint n = ff_utow(it->pszText, it->cchTextMax - 1, text, len, 0);
	it->pszText[n] = '\0';
}

/** Check/uncheck a 'dispinfo_item' object. */
static inline void ffui_view_dispinfo_check(LVITEMW *it, int checked)
{
	it->stateMask |= LVIS_STATEIMAGEMASK;
	it->state &= ~LVIS_STATEIMAGEMASK;
	it->state |= (checked) ? _FFUI_VIEW_CHECKED : _FFUI_VIEW_UNCHECKED;
}


#ifdef __cplusplus
struct ffui_viewitemxx : ffui_viewitem {
	ffui_viewitemxx(ffstr s) { ffmem_zero_obj(this); ffui_view_settextstr(this, &s); }
};

struct ffui_viewcolxx : ffui_viewcol {
	uint width() { return ffui_viewcol_width(this); }
	void width(uint val) { ffui_viewcol_setwidth(this, val); }
};

struct ffui_viewxx : ffui_view {
	int append(ffstr text) {
		ffui_viewitem vi = {};
		ffui_view_settextstr(&vi, &text);
		return ffui_view_append(this, &vi);
	}
	void set(int idx, int col, ffstr text) {
		ffui_viewitem vi = {};
		ffui_view_setindex(&vi, idx);
		ffui_view_settextstr(&vi, &text);
		ffui_view_set(this, col, &vi);
	}
	void update(uint first, int delta) {
		uint last = first;
		if (delta > 0) {
			last = first + 50;
		} else if (delta < 0) {
			last = first + 50;
			first = ffmax((int)first - 50, 0);
		}
		ffui_view_redraw(this, first, last);
	}
	void length(uint n, bool redraw) {
		uint flags = (redraw) ? 0 : LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL;
		ffui_ctl_send(this, LVM_SETITEMCOUNT, n, flags);
	}
	void clear() { ffui_view_clear(this); }
	int focused() { return ffui_view_focused(this); }
	ffslice selected() {
		ffvec sel = {};
		int i = -1;
		while (-1 != (i = ffui_view_selnext(this, i))) {
			*ffvec_pushT(&sel, uint) = i;
		}
		return *(ffslice*)&sel;
	}
	int selected_first() { return ffui_view_selnext(this, -1); }
	ffui_viewcolxx& column(int pos, ffui_viewcolxx *vc) {
		ffui_view_col(this, pos, vc);
		return *vc;
	}
	void column(int pos, ffui_viewcol &vc) { ffui_view_setcol(this, pos, &vc); }
};
#endif
