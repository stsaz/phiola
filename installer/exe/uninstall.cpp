/** phiola/Windows uninstaller
Simon Zolin, 2026 */

#define MTX_NAME  "Local\\phiola-uninstall"
#define DEL_MAX_FILES  100
#define CHILD_MTX_WAIT_MS  (60*1000)

#include <ffsys/environ.h>
#include <conf.h>
#include <utils.h>
#include <util/util.hpp>
#include <ffsys/error.h>
#include <ffsys/dirscan.h>
#include <ffsys/file.h>
#include <ffsys/path.h>
#include <ffsys/process.h>
#include <ffsys/winreg.h>
#include <ffsys/globals.h>
#include <ffbase/args.h>
#include <ffbase/fntree.h>
#include <ffbase/vector.h>

static int uninstall_spawn(const char *fn, const char *app_dir)
{
	xxptr tmp_exe;
	if (!(tmp_exe.ptr = ffenv_expand(NULL, NULL, 0, "%TMP%\\phiola-uninstall.exe")))
		return 1;

	xxvec data;
	if (fffile_readwhole(fn, &data, 1*1024*1024)
		|| fffile_writewhole(tmp_exe.ptr, (char*)data.ptr, data.len, 0))
		return 1;

	const char *argv[] = { tmp_exe.ptr, "-u", app_dir, NULL };

	xxstr tmp_dir = xxpath(tmp_exe.ptr).path();
	xxvec tmp_buf;
	tmp_buf.add_f("%S%Z", &tmp_dir);

	ffps_execinfo info = {};
	info.argv = argv;
	info.workdir = (char*)tmp_buf.ptr;
	info.in = info.out = info.err = INVALID_HANDLE_VALUE;
	if (FFPS_NULL == ffps_exec_info(tmp_exe.ptr, &info))
		return 1;

	return 0;
}

/** Verify this is true phiola installation:
. <dir> name is 'phiola-2'
. <dir>\phiola-gui.exe exists
. <dir>\shell\uninstall.exe is byte-identical to the running image. */
static int uninstall_verify(ffstr dir, const char *self_fn)
{
	if (!xxpath(dir).name().equals(DIR_NAME))
		return -1;

	xxvec fn, data, self;
	fn.add_f("%S\\%s%Z", &dir, EXE_NAME);
	if (!fffile_exists(fn.sz()))
		return -1;

	fn.len = dir.len;
	fn.add_f("\\%s%Z", UNINSTALL_EXE);
	if (fffile_readwhole(fn.sz(), &data, 1*1024*1024)
		|| fffile_readwhole(self_fn, &self, 1*1024*1024))
		return -1;
	if (data.len != self.len
		|| ffmem_cmp(data.ptr, self.ptr, data.len))
		return -1;

	return 0;
}

/** Delete a file at the given env-expanded path. */
static void uninstall_shortcut(const char *lnk)
{
	xxptr p;
	if (!(p.ptr = ffenv_expand(NULL, NULL, 0, lnk)))
		return;
	job(fffile_remove(p.ptr), p.ptr);
}

/** Scan the directory: delete files immediately, add subdirs to the tree.
Reparse points (junction/symlink) are removed as links.
limit: remaining N of entries allowed to process */
static int uninstall_dir_scan(fntree_block **blk, int *limit)
{
	int r = 0;
	size_t prefix;
	ffdirscan ds = {};
	xxvec fpath;
	ffstr path = fntree_path(*blk);
	if (ffdirscan_open(&ds, path.ptr, FFDIRSCAN_NOSORT))
		return -1;

	*limit -= ffdirscan_count(&ds);
	if (*limit < 0) {
		r = -1;
		goto end;
	}

	fpath.alloc<char>(path.len + 1 + 255 + 1);
	fpath.cat_f("%S\\", &path);
	prefix = fpath.len;
	const char *name;
	while ((name = ffdirscan_next(&ds))) {
		fpath.len = prefix;
		fpath.cat_f("%s", name);

		fffileinfo fi;
		if (fffile_info_path(fpath.sz(), &fi))
			continue;
		uint attr = fffileinfo_attr(&fi);

		if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
			if (fffile_isdir(attr)) {
				r |= job(ffdir_remove(fpath.sz()), fpath.sz());
			} else {
				r |= job(fffile_remove(fpath.sz()), fpath.sz());
			}
			continue;
		}

		if (fffile_isdir(attr)) {
			fntree_entry *e;
			if (!(e = fntree_addz(blk, name, 0)))
				continue;
			fntree_attach(e, fntree_create(fpath.str()));
		} else {
			r |= job(fffile_remove(fpath.sz()), fpath.sz());
		}
	}

end:
	ffdirscan_close(&ds);
	return r;
}

/** Recursively delete the directory tree.
Stop if more than 'limit' total entries are found (Note: some files may still be deleted). */
static int uninstall_dir(const char *dir, uint limit)
{
	fffileinfo fi;
	if (fffile_info_path(dir, &fi))
		return -1;
	if (fffileinfo_attr(&fi) & FILE_ATTRIBUTE_REPARSE_POINT) {
		return job(ffdir_remove(dir), dir); // If the root is a reparse point: remove the link only
	}

	int r = 0;
	fntree_block *root = fntree_create(FFSTR_Z(dir));
	fntree_block *blk = root;
	fntree_cursor cur = {};
	fntree_entry *e = NULL;

	// Scan; delete files; build the directory tree
	for (;;) {
		r |= uninstall_dir_scan(&blk, (int*)&limit);
		if ((int)limit < 0) {
			r = -1;
			goto end;
		}
		if (e)
			e->children = blk;

		if (!(e = fntree_cur_next_r(&cur, &blk)))
			break;
		blk = e->children;
	}

	// Delete directories in post-order (deepest first, root last)
	blk = root;
	ffmem_zero_obj(&cur);
	for (;;) {
		if (!(blk = _fntr_blk_next_r_post(&cur, blk)))
			break;
		r |= job(ffdir_remove(fntree_path(blk).ptr), fntree_path(blk).ptr);
	}

end:
	fntree_free_all(root);
	return r;
}

static int uninstall_mode(xxstr line, xxstr *dir)
{
	xxstr su;
	_ffargs_next(&line, &su); // Skip exe name
	_ffargs_next(&line, &su);
	if (!su.equals("-u"))
		return 0; // Interactive mode
	_ffargs_next(&line, dir);
	if (line.len)
		return 0; // Requires "-u DIR"
	return 1;
}

/** Check if in portable mode. */
static int uninstall_portable_check(ffstr app_dir)
{
	xxvec fn;
	fn.add_f("%S\\%s%Z", &app_dir, CONF_PORTABLE);
	return fffile_exists(fn.sz());
}

static inline int ffui_msgdlg_show(const char *title, const char *text, ffsize len, uint flags) {
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

#define ui_msg_warn(text, flags) \
	ffui_msgdlg_showz("phiola uninstaller", text, flags | MB_ICONWARNING)
#define ui_msg_info(text, flags) \
	ffui_msgdlg_showz("phiola uninstaller", text, flags | MB_ICONINFORMATION)

typedef HANDLE ffmtx;
#define FFMTX_NULL  NULL
#define FFMTX_CREATE  1

/** Create or open mutex object.
name: optional name
flags: FFMTX_CREATE or 0
Return FFMTX_NULL on error */
static inline ffmtx ffmtx_open(const char *name, uint flags)
{
	wchar_t wbuf[256], *wname = NULL;
	if (name != NULL
		&& NULL == (wname = ffsz_alloc_buf_utow(wbuf, FF_COUNT(wbuf), name)))
		return FFMTX_NULL;

	ffmtx h;
	if (flags & FFMTX_CREATE)
		h = CreateMutexW(NULL, 0, wname);
	else
		h = OpenMutexW(SYNCHRONIZE, 0, wname);

	if (wname != wbuf)
		ffmem_free(wname);
	return h;
}

static inline int ffmtx_wait(ffmtx h, uint time_ms)
{
	int r = WaitForSingleObject(h, time_ms);
	if (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED)
		r = 0;
	else if (r == WAIT_TIMEOUT)
		SetLastError(WSAETIMEDOUT);
	return r;
}

#ifdef FF_DEBUG
int main()
#else
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#endif
{
	xxptr cmd_line(ffsz_alloc_wtou(GetCommandLineW()));
	xxstr dir;
	uint perform = uninstall_mode(cmd_line.ptr, &dir);
	const char *e = NULL;
	int r = 0;

	char fn_buf[4096];
	const char *fn;
	if (!(fn = ffps_filename(fn_buf, sizeof(fn_buf), NULL))) {
		e = "Failed to determine the uninstaller location.\n"
			"The uninstaller will exit now.";
		goto err;
	}

	ffmtx mtx;
	if (FFMTX_NULL == (mtx = ffmtx_open(MTX_NAME, FFMTX_CREATE))
		|| ffmtx_wait(mtx, (!perform) ? 0 : CHILD_MTX_WAIT_MS)) {

		if (!perform && mtx != FFMTX_NULL)
			e = "Uninstallation is already in progress";
		else if (!perform)
			e = "Failed to start the uninstaller.\n"
				"Please try again.";
		else
			e = "Uninstallation failed\n"
				"Please try again.";
		goto err;
	}

	if (!perform)
		dir = xxpath(xxpath(fn).path()).path();
	if (uninstall_portable_check(dir)) {
		e = "This phiola installation is in portable mode.\n"
			"The uninstaller will exit now.";
		goto err;
	}

	if (!perform) {
		if (IDYES != ui_msg_warn(
			"This will remove phiola from your computer:\n"
			"- delete the installation directory\n"
			"- remove file type associations\n"
			"\n"
			"It will NOT delete your phiola settings and recorded files.\n"
			"Make sure phiola is closed.\n"
			"Continue?"
			, MB_YESNO))
			return 0;

		if (uninstall_spawn(fn, xxvec().add_f("%S%Z", &dir).sz())) {
			e = "Failed to start the uninstaller.\n"
				"Please try again.";
			goto err;
		}
		return 0;
	}

	if (uninstall_verify(dir, fn)) {
		e = "phiola could not be uninstalled.\n"
			"The installation directory is missing or has been modified.";
		goto err;
	}

	vlog.alloc<char>(4096);
	r |= shell_ext_unreg(EXE_NAME, (char*)phi_exts, sizeof(phi_exts[0]), FF_COUNT(phi_exts));
	r |= env_path_remove(dir);
	dir.ptr[dir.len] = '\0';
	r |= uninstall_dir(dir.ptr, DEL_MAX_FILES);
	uninstall_shortcut(LINK_NAME);

	(void)fffile_writewhole(xxvec(ffenv_expand(NULL, NULL, 0, "%TMP%\\phiola-uninstall.log")).strz(), (char*)vlog.ptr, vlog.len, 0);
	if (r) {
		e = "Uninstall finished with errors.\n"
			"Some phiola files or settings may remain.\n"
			"Please check the installation directory and remove anything left behind manually.";
		goto err;
	}
	ui_msg_info("phiola was uninstalled.", 0);
	return 0;

err:
	ui_msg_warn(e, 0);
	return 1;
}
