/** GUI-winapi: treeview
2014, Simon Zolin */

#pragma once
#include "view.h"

static inline int ffui_tree_create(ffui_ctl *c, void *parent)
{
	if (0 != _ffui_ctl_create(c, FFUI_UID_TREEVIEW, ((ffui_ctl*)parent)->h, 0, 0))
		return 1;

#if FF_WIN >= 0x0600
	int n = TVS_EX_DOUBLEBUFFER;
	TreeView_SetExtendedStyle(c->h, n, n);
#endif

	return 0;
}

typedef struct ffui_tvitem {
	TVITEMW ti;
	wchar_t wtext[255];
	wchar_t *w;
} ffui_tvitem;

static inline void ffui_tree_reset(ffui_tvitem *it)
{
	it->ti.mask = 0;
	if (it->w != it->wtext && it->w != NULL) {
		ffmem_free(it->w);
		it->w = NULL;
	}
}

#define ffui_tree_settext_q(it, textz) \
do { \
	(it)->ti.mask |= TVIF_TEXT; \
	(it)->ti.pszText = (wchar_t*)textz; \
} while (0)

static inline void ffui_tree_settext(ffui_tvitem *it, const char *text, ffsize len)
{
	ffsize n = FF_COUNT(it->wtext) - 1;
	if (NULL == (it->w = ffs_utow(it->wtext, &n, text, len)))
		return;
	it->w[n] = '\0';
	ffui_tree_settext_q(it, it->w);
}
#define ffui_tree_settextstr(it, str)  ffui_tree_settext(it, (str)->ptr, (str)->len)
#define ffui_tree_settextz(it, sz)  ffui_tree_settext(it, sz, ffsz_len(sz))

#define ffui_tree_setimg(it, img_idx) \
do { \
	(it)->ti.mask |= TVIF_IMAGE | TVIF_SELECTEDIMAGE; \
	(it)->ti.iImage = (img_idx); \
	(it)->ti.iSelectedImage = (img_idx); \
} while (0)

#define ffui_tree_setparam(it, param) \
do { \
	(it)->ti.mask |= TVIF_PARAM; \
	(it)->ti.lParam = (LPARAM)(param); \
} while (0)

#define ffui_tree_setexpand(it, val) \
do { \
	(it)->ti.mask |= TVIF_STATE; \
	(it)->ti.stateMask |= TVIS_EXPANDED; \
	(it)->ti.state |= (val) ? TVIS_EXPANDED : 0; \
} while (0)

#define ffui_tree_param(it)  ((void*)(it)->ti.lParam)

//insert after:
#define FFUI_TREE_FIRST  ((void*)-0x0FFFF)
#define FFUI_TREE_LAST  ((void*)-0x0FFFE)

static inline void* ffui_tree_ins(ffui_view *v, void *parent, void *after, ffui_tvitem *it)
{
	TVINSERTSTRUCTW ins = {};
	ins.hParent = (HTREEITEM)parent;
	ins.hInsertAfter = (HTREEITEM)after;
	ins.item = it->ti;
	void *r = (void*)ffui_send(v->h, TVM_INSERTITEM, 0, &ins);
	ffui_tree_reset(it);
	return r;
}
#define ffui_tree_append(v, parent, it)  ffui_tree_ins(v, parent, FFUI_TREE_LAST, it)

#define ffui_tree_remove(t, item)  ffui_ctl_send(t, TVM_DELETEITEM, 0, item)

static inline void ffui_tree_get(ffui_view *v, HTREEITEM pitem, ffui_tvitem *it)
{
	it->ti.hItem = pitem;
	ffui_send(v->h, TVM_GETITEM, 0, &it->ti);
}

static inline char* ffui_tree_text(ffui_view *t, void *item)
{
	wchar_t buf[255];
	TVITEMW it = {};
	it.mask = TVIF_TEXT;
	it.pszText = buf;
	it.cchTextMax = FF_COUNT(buf);
	it.hItem = (HTREEITEM)item;
	if (ffui_ctl_send(t, TVM_GETITEM, 0, &it))
		return ffsz_alloc_wtou(buf);
	return NULL;
}

#define ffui_tree_clear(t)  ffui_ctl_send(t, TVM_DELETEITEM, 0, TVI_ROOT)

#define ffui_tree_count(t)  ffui_ctl_send(t, TVM_GETCOUNT, 0, 0)

#define ffui_tree_expand(t, item)  ffui_ctl_send(t, TVM_EXPAND, TVE_EXPAND, item)
#define ffui_tree_collapse(t, item)  ffui_ctl_send(t, TVM_EXPAND, TVE_COLLAPSE, item)

#define ffui_tree_makevisible(t, item)  ffui_ctl_send(t, TVM_ENSUREVISIBLE, 0, item)

#define ffui_tree_parent(t, item)  ((void*)ffui_ctl_send(t, TVM_GETNEXTITEM, TVGN_PARENT, item))
#define ffui_tree_prev(t, item)  ((void*)ffui_ctl_send(t, TVM_GETNEXTITEM, TVGN_PREVIOUS, item))
#define ffui_tree_next(t, item)  ((void*)ffui_ctl_send(t, TVM_GETNEXTITEM, TVGN_NEXT, item))
#define ffui_tree_child(t, item)  ((void*)ffui_ctl_send(t, TVM_GETNEXTITEM, TVGN_CHILD, item))

#define ffui_tree_focused(t)  ((void*)ffui_ctl_send(t, TVM_GETNEXTITEM, TVGN_CARET, NULL))
#define ffui_tree_select(t, item)  ffui_ctl_send(t, TVM_SELECTITEM, TVGN_CARET, item)

#define ffui_tree_seticonlist(t, il)  TreeView_SetImageList((t)->h, (il)->h, TVSIL_NORMAL)
