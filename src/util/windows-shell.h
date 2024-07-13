/** Windows shell utils
2020, Simon Zolin */

/*
ffui_file_del
ffui_openfolder ffui_openfolder1
ffui_createlink
ffui_shellexec ffui_exec
ffui_clipbd_set ffui_clipbd_setfile
*/

#pragma once
#ifndef COBJMACROS
	#define COBJMACROS
#endif
#ifndef CINTERFACE
	#define CINTERFACE
#endif
#include <ffsys/path.h>
#include <ffsys/dir.h>
#include <ffsys/error.h>
#include <ffbase/vector.h>
#include <shlobj.h>
#include <objidl.h>
#include <shobjidl.h>
#include <shlguid.h>

/** Prepare double-null terminated string array from char*[]
dst: "el1 \0 el2 \0 \0" */
static ffsize _ff_arrzz_copy(wchar_t *dst, ffsize cap, const char *const *arr, ffsize n)
{
	if (dst == NULL) {
		cap = 0;
		for (ffsize i = 0;  i != n;  i++) {
			cap += ffsz_utow(NULL, 0, arr[i]);
		}
		return cap + 1;
	}

	ffsize k = 0;
	for (ffsize i = 0;  i != n;  i++) {
		k += ffsz_utow(&dst[k], cap - k, arr[i]);
	}
	dst[k] = '\0';
	return k+1;
}

enum FFUI_FILE_F {
	FFUI_FILE_TRASH = FOF_ALLOWUNDO,
};

/** Delete a file
flags: enum FFUI_FILE_F */
static inline int ffui_file_del(const char *const *names, ffsize cnt, ffuint flags)
{
	if (!cnt) return 0;

	if (flags & FFUI_FILE_TRASH) {
		for (ffsize i = 0;  i != cnt;  i++) {
			if (!ffpath_abs(names[i], ffsz_len(names[i]))) {
				fferr_set(ERROR_INVALID_PARAMETER);
				return -1; // protect against permanently deleting files with non-absolute names
			}
		}
	}

	SHFILEOPSTRUCTW fs = {};
	ffsize cap = _ff_arrzz_copy(NULL, 0, names, cnt);
	if (NULL == (fs.pFrom = ffws_alloc(cap)))
		return -1;
	_ff_arrzz_copy((wchar_t*)fs.pFrom, cap, names, cnt);

	fs.wFunc = FO_DELETE;
	fs.fFlags = flags;
	int r = SHFileOperationW(&fs);

	ffmem_free((void*)fs.pFrom);
	return r;
}

#ifdef __cplusplus
#define _FFCOM_ID(id)  id
#else
#define _FFCOM_ID(id)  &id
#endif

/** Create .lnk file. */
static inline int ffui_createlink(const char *target, const char *linkname)
{
	HRESULT r;
	IShellLinkW *sl = NULL;
	IPersistFile *pf = NULL;
	wchar_t ws[255], *w = ws;
	ffsize n = FF_COUNT(ws);

	if (0 != (r = CoCreateInstance(_FFCOM_ID(CLSID_ShellLink), NULL, CLSCTX_INPROC_SERVER, _FFCOM_ID(IID_IShellLinkW), (void**)&sl)))
		goto end;

	if (NULL == (w = ffs_utow(ws, &n, target, -1)))
		goto end;
	IShellLinkW_SetPath(sl, w);
	if (w != ws)
		ffmem_free(w);

	if (0 != (r = IShellLinkW_QueryInterface(sl, _FFCOM_ID(IID_IPersistFile), (void**)&pf)))
		goto end;

	n = FF_COUNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, linkname, -1)))
		goto end;
	if (0 != (r = IPersistFile_Save(pf, w, TRUE)))
		goto end;

end:
	if (w != ws)
		ffmem_free(w);
	if (sl != NULL)
		IShellLinkW_Release(sl);
	if (pf != NULL)
		IPersistFile_Release(pf);
	return r;
}

/**
@flags: SW_* */
static inline int ffui_shellexec(const char *filename, ffuint flags)
{
	wchar_t *w, ws[4096];
	ffsize n = FF_COUNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, filename, -1)))
		return -1;

	int r = (ffsize)ShellExecuteW(NULL, L"open", w, NULL, NULL, flags);
	if (w != ws)
		ffmem_free(w);
	return (r <= 32) ? -1 : 0;
}

#define ffui_exec(filename)  ffui_shellexec(filename, SW_SHOWNORMAL)

/** Return 0 on success. */
static inline int ffui_clipbd_set(const char *s, ffsize len)
{
	HGLOBAL glob;
	ffsize n = ff_utow(NULL, 0, s, len, 0);
	if (NULL == (glob = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE, (n + 1) * sizeof(wchar_t))))
		return -1;
	wchar_t *buf = (wchar_t*)GlobalLock(glob);
	n = ff_utow(buf, n + 1, s, len, 0);
	buf[n] = '\0';
	GlobalUnlock(glob);

	if (!OpenClipboard(NULL))
		goto fail;
	EmptyClipboard();
	if (!SetClipboardData(CF_UNICODETEXT, glob))
		goto fail;
	CloseClipboard();

	return 0;

fail:
	CloseClipboard();
	GlobalFree(glob);
	return -1;
}

static inline int ffui_clipbd_setfile(const char *const *names, ffsize cnt)
{
	HGLOBAL glob;
	struct df_s {
		DROPFILES df;
		wchar_t names[0];
	} *s;

	ffsize cap = _ff_arrzz_copy(NULL, 0, names, cnt);
	if (NULL == (glob = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE, sizeof(DROPFILES) + cap * sizeof(wchar_t))))
		return -1;

	s = (struct df_s*)GlobalLock(glob);
	ffmem_zero_obj(&s->df);
	s->df.pFiles = sizeof(DROPFILES);
	s->df.fWide = 1;
	_ff_arrzz_copy(s->names, cap, names, cnt);
	GlobalUnlock(glob);

	if (!OpenClipboard(NULL))
		goto fail;
	EmptyClipboard();
	if (!SetClipboardData(CF_HDROP, glob))
		goto fail;
	CloseClipboard();

	return 0;

fail:
	CloseClipboard();
	GlobalFree(glob);
	return -1;
}

/** Convert '/' -> '\\' */
static void path_backslash(wchar_t *path)
{
	ffuint n;
	for (n = 0;  path[n] != '\0';  n++) {
		if (path[n] == '/')
			path[n] = '\\';
	}
}

static inline int ffui_openfolder(const char *const *items, ffsize selcnt)
{
	ITEMIDLIST *dir = NULL;
	int r = -1;
	wchar_t *pathz = NULL;
	ffvec norm = {}, sel = {};

	// normalize directory path
	ffstr path;
	ffstr_setz(&path, items[0]);
	if (selcnt != 0)
		ffpath_splitpath(path.ptr, path.len, &path, NULL);
	if (NULL == ffvec_alloc(&norm, path.len + 1, 1))
		goto done;
	if (0 >= (norm.len = ffpath_normalize((char*)norm.ptr, norm.cap, path.ptr, path.len, FFPATH_FORCE_BACKSLASH))) {
		fferr_set(ERROR_INVALID_PARAMETER);
		goto done;
	}
	*ffvec_pushT(&norm, char) = '\\';

	// get directory object
	ffsize n;
	if (NULL == (pathz = ffs_utow(NULL, &n, (char*)norm.ptr, norm.len)))
		goto done;
	pathz[n] = '\0';
	if (NULL == (dir = ILCreateFromPathW(pathz)))
		goto done;

	// fill array with the files to be selected
	if (selcnt == 0)
		selcnt = 1;
	if (NULL == ffvec_allocT(&sel, selcnt, ITEMIDLIST*))
		goto done;
	for (ffsize i = 0;  i != selcnt;  i++) {
		if (NULL == (pathz = ffs_utow(NULL, NULL, items[i], -1)))
			goto done;
		path_backslash(pathz);

		ITEMIDLIST *dir;
		if (NULL == (dir = ILCreateFromPathW(pathz)))
			goto done;
		*ffvec_pushT(&sel, ITEMIDLIST*) = dir;
		ffmem_free(pathz);  pathz = NULL;
	}

	r = SHOpenFolderAndSelectItems(dir, selcnt, (const ITEMIDLIST **)sel.ptr, 0);

done:
	if (dir != NULL) ILFree(dir);

	ITEMIDLIST **it;
	FFSLICE_WALK(&sel, it) {
		ILFree(*it);
	}
	ffvec_free(&sel);

	ffvec_free(&norm);
	ffmem_free(pathz);
	return r;
}

static inline int ffui_openfolder1(const char *path)
{
	return ffui_openfolder(&path, 0);
}
