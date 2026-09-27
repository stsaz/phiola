/** phiola/Windows uninstaller
Simon Zolin, 2026 */

#define MTX_NAME  "Local\\phiola-uninstall"
#define LOG_PATH  "%TMP%\\phiola-uninstall.log"
#define TMP_EXE_PATH  "%TMP%\\phiola-uninstall.exe"
#define CHILD_MTX_WAIT_MS  (60*1000)

#include <util/windows-shell.h>
#include <ffsys/environ.h>
#include <conf.h>
#include <utils.h>
#include <util/util.hpp>
#include <ffsys/error.h>
#include <ffsys/file.h>
#include <ffsys/path.h>
#include <ffsys/process.h>
#include <ffsys/winreg.h>
#include <ffsys/globals.h>
#include <ffbase/args.h>
#include <ffbase/vector.h>

static int uninstall_spawn(const char *fn, const char *app_dir)
{
	xxptr tmp_exe;
	if (!(tmp_exe.ptr = ffenv_expand(NULL, NULL, 0, TMP_EXE_PATH)))
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

static xxvec vlog;
static int job(int r, const char *path)
{
	vlog.add_f("%s %s\r\n"
		, (!r) ? "OK " : "ERR", path);
	return r;
}

/** Delete a file at the given env-expanded path. */
static void uninstall_shortcut(const char *lnk)
{
	xxptr p;
	if (!(p.ptr = ffenv_expand(NULL, NULL, 0, lnk)))
		return;
	job(fffile_remove(p.ptr), p.ptr);
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
	ffmtx mtx = FFMTX_NULL;

	char fn_buf[4096];
	const char *fn;
	if (!(fn = ffps_filename(fn_buf, sizeof(fn_buf), NULL))) {
		e = "Failed to determine the uninstaller location.\n"
			"The uninstaller will exit now.";
		goto err;
	}

	if (FFMTX_NULL == (mtx = ffmtx_open(MTX_NAME, FFMTX_CREATE))
		|| ffmtx_wait(mtx, (!perform) ? 0 : CHILD_MTX_WAIT_MS)) {

		if (!perform && mtx != FFMTX_NULL)
			e = "Uninstallation is already in progress.";
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
			goto end;

		if (uninstall_spawn(fn, xxvec().add_f("%S%Z", &dir).sz())) {
			e = "Failed to start the uninstaller.\n"
				"Please try again.";
			goto err;
		}
		goto end;
	}

	if (uninstall_verify(dir, fn)) {
		e = "phiola could not be uninstalled.\n"
			"The installation directory is missing or has been modified.";
		goto err;
	}

	vlog.alloc<char>(4096);
	r |= shell_ext_unreg("phiola", EXE_NAME, (char*)phi_exts, sizeof(phi_exts[0]), FF_COUNT(phi_exts), job);
	r |= env_path_remove(dir, job);
	dir.ptr[dir.len] = '\0';
	r |= dir_remove_r(dir.ptr, DEL_MAX_FILES, job);
	uninstall_shortcut(START_MENU_LINK);
	uninstall_shortcut(LINK_NAME);

	(void)fffile_writewhole(xxvec().acquire(ffenv_expand(NULL, NULL, 0, LOG_PATH)).strz(), (char*)vlog.ptr, vlog.len, 0);
	if (r) {
		e = "Uninstall finished with errors.\n"
			"Some phiola files or settings may remain.\n"
			"Please check the installation directory and delete any remaining files manually.";
		goto err;
	}
	ui_msg_info("phiola was uninstalled.", 0);
	goto end;

err:
	ui_msg_warn(e, 0);
	r = 1;

end:
	ffmtx_close(mtx);
	return !!r;
}
