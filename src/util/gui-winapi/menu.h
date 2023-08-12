/** GUI-winapi: main/popup menu
2014, Simon Zolin */

#pragma once
#include "winapi.h"

typedef struct ffui_menu {
	HMENU h;
} ffui_menu;

static inline void ffui_menu_createmain(ffui_menu *m)
{
	m->h = CreateMenu();
}

static inline int ffui_menu_create(ffui_menu *m)
{
	return 0 == (m->h = CreatePopupMenu());
}

#define ffui_menu_show(m, x, y, hwnd) \
	TrackPopupMenuEx((m)->h, 0, x, y, hwnd, NULL)


typedef MENUITEMINFOW ffui_menuitem;

static inline void ffui_menu_itemreset(ffui_menuitem *mi)
{
	if (mi->dwTypeData != NULL)
		ffmem_free(mi->dwTypeData);
	ffmem_zero_obj(mi);
}

#define ffui_menu_setcmd(mi, cmd) \
do { \
	(mi)->fMask |= MIIM_ID; \
	(mi)->wID = (cmd); \
} while(0)

#define ffui_menu_setsubmenu(mi, hsub) \
do { \
	(mi)->fMask |= MIIM_SUBMENU; \
	(mi)->hSubMenu = (hsub); \
} while(0)

#define ffui_menu_setbmp(mi, hbmp) \
do { \
	(mi)->fMask |= MIIM_BITMAP; \
	(mi)->hbmpItem = (hbmp); \
} while(0)

enum FFUI_MENUSTATE {
	FFUI_MENU_CHECKED = MFS_CHECKED,
	FFUI_MENU_DEFAULT = MFS_DEFAULT,
	FFUI_MENU_DISABLED = MFS_DISABLED,
};

#define ffui_menu_addstate(mi, state) \
do { \
	(mi)->fMask |= MIIM_STATE; \
	(mi)->fState |= (state); \
} while(0)

#define ffui_menu_clearstate(mi, state) \
do { \
	(mi)->fMask |= MIIM_STATE; \
	(mi)->fState &= ~(state); \
} while(0)

enum FFUI_MENUTYPE {
	FFUI_MENU_SEPARATOR = MFT_SEPARATOR,
	FFUI_MENU_RADIOCHECK = MFT_RADIOCHECK,
};

#define ffui_menu_settype(mi, type) \
do { \
	(mi)->fMask |= MIIM_FTYPE; \
	(mi)->fType = (type); \
} while(0)

#define ffui_menu_settext_q(mi, sz) \
do { \
	(mi)->fMask |= MIIM_STRING; \
	(mi)->dwTypeData = (sz); \
} while(0)

static inline int ffui_menu_settext(ffui_menuitem *mi, const char *s, ffsize len)
{
	wchar_t *w;
	ffsize n;
	if (NULL == (w = ffs_utow(NULL, &n, s, len)))
		return -1;
	w[n] = '\0';
	ffui_menu_settext_q(mi, w);
	// ffmem_free(w)
	return 0;
}
#define ffui_menu_settextz(mi, sz)  ffui_menu_settext(mi, sz, ffsz_len(sz))
#define ffui_menu_settextstr(mi, str)  ffui_menu_settext(mi, (str)->ptr, (str)->len)

static inline int ffui_menu_ins(ffui_menu *m, int pos, ffui_menuitem *mi)
{
	mi->cbSize = sizeof(MENUITEMINFOW);
	int r = !InsertMenuItemW(m->h, pos, 1, mi);
	ffui_menu_itemreset(mi);
	return r;
}

#define ffui_menu_append(m, mi)  ffui_menu_ins(m, -1, mi)

#define ffui_menu_rm(m, pos)  RemoveMenu((m)->h, i, MF_BYPOSITION)

static inline int ffui_menu_set(ffui_menu *m, int pos, ffui_menuitem *mi)
{
	mi->cbSize = sizeof(MENUITEMINFOW);
	int r = !SetMenuItemInfoW(m->h, pos, 1, mi);
	ffui_menu_itemreset(mi);
	return r;
}

static inline int ffui_menu_set_byid(ffui_menu *m, int id, ffui_menuitem *mi)
{
	mi->cbSize = sizeof(MENUITEMINFOW);
	int r = !SetMenuItemInfoW(m->h, id, 0, mi);
	ffui_menu_itemreset(mi);
	return r;
}

static inline int ffui_menu_get_byid(ffui_menu *m, int id, ffui_menuitem *mi)
{
	mi->cbSize = sizeof(MENUITEMINFOW);
	int r = !GetMenuItemInfoW(m->h, id, 0, mi);
	return r;
}

static inline int ffui_menu_get(ffui_menu *m, int pos, ffui_menuitem *mi)
{
	mi->cbSize = sizeof(MENUITEMINFOW);
	return !GetMenuItemInfoW(m->h, pos, 1, mi);
}

static inline int ffui_menu_destroy(ffui_menu *m)
{
	int r = 0;
	if (m->h != 0) {
		r = !DestroyMenu(m->h);
		m->h = 0;
	}
	return r;
}
