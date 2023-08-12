/** GUI-winapi: standard dialogs
2014, Simon Zolin */

#pragma once
#include "winapi.h"

typedef struct ffui_dialog {
	OPENFILENAMEW of;
	wchar_t *names, *pname;
	char *name;
} ffui_dialog;

static inline void ffui_dlg_init(ffui_dialog *d)
{
	ffmem_zero_obj(d);
	d->of.lStructSize = sizeof(d->of);
	d->of.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
}

static inline void ffui_dlg_title(ffui_dialog *d, const char *title, ffsize len)
{
	ffsize n;
	ffmem_free((void*)d->of.lpstrTitle);
	if (NULL == (d->of.lpstrTitle = ffs_utow(NULL, &n, title, len)))
		return;
	wchar_t *w = (wchar_t*)d->of.lpstrTitle;
	w[n] = '\0';
}
#define ffui_dlg_titlez(d, sz)  ffui_dlg_title(d, sz, ffsz_len(sz))

static inline void ffui_dlg_filter(ffui_dialog *d, const char *title, ffsize len)
{
	ffmem_free((void*)d->of.lpstrFilter);
	d->of.lpstrFilter = ffs_utow(NULL, NULL, title, len);
}

/** Set filter index. */
#define ffui_dlg_nfilter(d, filter_index)  ((d)->of.nFilterIndex = (filter_index) + 1)

#define ffui_dlg_multisel(d)  ((d)->of.Flags |= OFN_ALLOWMULTISELECT)

static char* ffui_dlg_nextname(ffui_dialog *d);

/**
Return file name;  NULL on error. */
/* multisel: "dir \0 name1 \0 name2 \0 \0"
   singlesel: "name \0" */
static inline char* ffui_dlg_open(ffui_dialog *d, ffui_wnd *wnd)
{
	wchar_t *w;
	ffsize cap = ((d->of.Flags & OFN_ALLOWMULTISELECT) ? 64*1024 : 4096);
	if (NULL == (w = ffws_alloc(cap)))
		return NULL;
	w[0] = '\0';

	d->of.hwndOwner = ((ffui_ctl*)wnd)->h;
	d->of.lpstrFile = w;
	d->of.nMaxFile = cap;
	if (!GetOpenFileNameW(&d->of)) {
		ffmem_free(w);
		return NULL;
	}

	ffmem_free(d->names);
	d->names = d->pname = w;

	if ((d->of.Flags & OFN_ALLOWMULTISELECT) && w[d->of.nFileOffset - 1] == '\0') {
		d->pname += d->of.nFileOffset; //skip directory

	} else {
		d->pname += wcslen(w); //for ffui_dlg_nextname() to return NULL
		ffmem_free(d->name);
		if (NULL == (d->name = ffsz_alloc_wtou(w)))
			return NULL;
		return d->name;
	}

	return ffui_dlg_nextname(d);
}

/**
@fn: default filename */
static inline char* ffui_dlg_save(ffui_dialog *d, ffui_wnd *wnd, const char *fn, ffsize fnlen)
{
	wchar_t ws[4096];
	ffsize n = 0;

	if (fn != NULL)
		n = ff_utow(ws, FF_COUNT(ws), fn, fnlen, 0);
	ws[n] = '\0';

	d->of.hwndOwner = ((ffui_ctl*)wnd)->h;
	d->of.lpstrFile = ws;
	d->of.nMaxFile = FF_COUNT(ws);
	if (!GetSaveFileNameW(&d->of))
		return NULL;

	ffmem_free(d->name);
	if (NULL == (d->name = ffsz_alloc_wtou(ws)))
		return NULL;
	return d->name;
}

static inline void ffui_dlg_destroy(ffui_dialog *d)
{
	ffmem_free(d->names);
	ffmem_free(d->name);
	ffmem_free((void*)d->of.lpstrTitle);
	ffmem_free((void*)d->of.lpstrFilter);
}

/** Get the next file name (for a dialog with multiselect). */
static inline char* ffui_dlg_nextname(ffui_dialog *d)
{
	ffmem_free(d->name); d->name = NULL; //free the previous name

	ffsize cap, namelen = wcslen(d->pname);
	if (namelen == 0) {
		ffmem_free(d->names); d->names = NULL;
		return NULL;
	}

	cap = d->of.nFileOffset + ff_wtou(NULL, 0, d->pname, namelen, 0) + 1;
	if (NULL == (d->name = (char*)ffmem_alloc(cap)))
		return NULL;

	ffs_format(d->name, cap, "%q\\%*q", d->names, namelen + 1, d->pname);
	d->pname += namelen + 1;
	return d->name;
}


enum FFUI_MSGDLG {
	FFUI_MSGDLG_INFO = MB_ICONINFORMATION,
	FFUI_MSGDLG_WARN = MB_ICONWARNING,
	FFUI_MSGDLG_ERR = MB_ICONERROR,
};

static inline int ffui_msgdlg_show(const char *title, const char *text, ffsize len, uint flags)
{
	int r = -1;
	wchar_t *w = NULL, *wtit = NULL;
	ffsize n;

	if (NULL == (w = ffs_utow(NULL, &n, text, len)))
		goto done;
	w[n] = '\0';

	if (NULL == (wtit = ffs_utow(NULL, NULL, title, -1)))
		goto done;

	r = MessageBoxW(NULL, w, wtit, flags);

done:
	ffmem_free(wtit);
	ffmem_free(w);
	return r;
}
#define ffui_msgdlg_showz(title, text, flags)  ffui_msgdlg_show(title, text, ffsz_len(text), flags)
