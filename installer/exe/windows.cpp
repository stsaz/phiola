/** phiola/Windows installer
Simon Zolin, 2024 */

#define DEFAULT_INSTALL_PATH  "%USERPROFILE%\\" DIR_NAME
#define TITLE  "Set up phiola v" PHI_VERSION_STR
#define HOMEPAGE_URL  "https://github.com/stsaz/phiola"
#define RES_UI  MAKEINTRESOURCEA(1)
#define RES_PKG  MAKEINTRESOURCEA(2)
#define RES_UNINST  MAKEINTRESOURCEA(3)
#include <conf.h>

#define MSG_TITLE  "phiola setup"
#define E_EXISTS  "The specified directory already exists"
#define E_NO_PATH  "The specified path does not exist"
#define E_ABS_PATH  "The install path must be a full path"
#define E_DIR_NAME  "The directory name must be \"phiola-2\", but you specified"
#define E_CORRUPT  "The installer file is corrupted.  Please redownload it."

#include <util/windows-shell.h>
#ifdef FF_DEBUG
#include <ffsys/std.h>
#endif
#include <utils.h>
#include <phiola.h>
#include <ffgui/winapi/loader.h>
#include <ffgui/loader.h>
#include <ffgui/gui.hpp>
#include <ffsys/dylib.h>
#include <ffsys/environ.h>
#include <ffsys/globals.h>

#define INCLUDE_ACTIONS(_) \
	_(A_INSTALL), \
	_(A_BROWSE), \
	_(A_HOMEPAGE), \
	_(A_CLOSE), \
	_(A_CB_PORTABLE), \
	_(A_DIR_CHANGED),

#define _(id) id
enum {
	A_NONE = 999,
	INCLUDE_ACTIONS(_)
};
#undef _

struct installer {
	struct wmain {
		ffui_windowxx	wnd;
		ffui_labelxx	ldir, lurl, lstatus;
		ffui_editxx		edir;
		ffui_checkboxxx	cb_portable, cb_shortcut, cb_environ, cb_start, cb_defaultapp;
		ffui_buttonxx	bbrowse, binstall;
		ffui_image		ico;
	} wmain;

	struct dark_theme theme;

	HGLOBAL	hpkg;
	ffstr	pkg;
	uint	done;

	HGLOBAL	hres_uninst_exe;
	ffstr	uninst_exe;

	~installer()
	{
		ffui_res_close(hpkg);
		ffui_res_close(hres_uninst_exe);
	}

	static void* gui_ctl_find(void *udata, const ffstr *name)
	{
		#define _(m) FFUI_LDR_CTL(struct wmain, m)
		static const ffui_ldr_ctl wmain_ctls[] = {
			_(wnd),
			_(ldir), _(edir), _(lstatus), _(bbrowse),
			_(cb_portable),
			_(cb_shortcut),
			_(cb_environ),
			_(cb_start),
			_(cb_defaultapp),
			_(binstall),
			_(lurl),
			_(ico),
			FFUI_LDR_CTL_END
		};
		#undef _

		static const ffui_ldr_ctl top_ctls[] = {
			FFUI_LDR_CTL3(struct installer, wmain, wmain_ctls),
			FFUI_LDR_CTL_END
		};

		return ffui_ldr_findctl(top_ctls, udata, name);
	}

	static int gui_cmd_find(void *udata, const ffstr *name)
	{
		static const char action_str[][24] = {
			#define _(id)  #id
			INCLUDE_ACTIONS(_)
			#undef _
		};

		for (uint i = 0;  i != FF_COUNT(action_str);  i++) {
			if (ffstr_eqz(name, action_str[i]))
				return A_NONE+1 + i;
		}
		return 0;
	}

	enum INST_T {
		INST_NONE, // target dir doesn't exist
		INST_EXISTS, // target dir exists, not a phiola installation
		INST_PORTABLE, // phiola portable installation
		INST_UPGRADE, // phiola installation, different version
		INST_UPGRADE_SAME, // phiola installation, same version
	};

	/** Get status of the target app dir.
	Return enum INST_T. */
	int upgrade_check(xxvec &buf, xxstr dir, xxvec *version) {
		int rc = INST_NONE;
		ffdl phi_dll = FFDL_NULL;
		struct phi_core_conf conf = {};
		phi_core *core = NULL;
		typedef phi_core* (*phi_core_create_t)(struct phi_core_conf *);
		phi_core_create_t phi_core_create = NULL;
		void (*phi_core_destroy)() = NULL;

		buf.clear().realloc<char>(dir.len + 1 + 255 + 1);
		buf.cat(dir);
		if (!fffile_exists(buf.sz())) {
			rc = INST_NONE;
			goto end;
		}

		buf.cat_f("\\%s", EXE_NAME);
		if (!fffile_exists(buf.sz())) {
			rc = INST_EXISTS;
			goto end;
		}

		buf.len = dir.len;
		buf.cat("\\libphiola.dll");
		if (FFDL_NULL == (phi_dll = ffdl_open(buf.sz(), 0))) {
			rc = INST_EXISTS;
			goto end;
		}

		phi_core_create = (phi_core_create_t)ffdl_addr(phi_dll, "phi_core_create");
		phi_core_destroy = (void (*)())ffdl_addr(phi_dll, "phi_core_destroy");
		if (!phi_core_create || !phi_core_destroy) {
			rc = INST_EXISTS;
			goto end;
		}

		core = phi_core_create(&conf);
		if (!core || !core->version_str
			|| ffsz_len(core->version_str) > FFS_LEN("0.00-beta00")) {
			rc = INST_EXISTS;
			goto end;
		}

		rc = INST_UPGRADE;
		if (version)
			version->add(core->version_str);
		if (ffsz_eq(core->version_str, PHI_VERSION_STR))
			rc = INST_UPGRADE_SAME;

		buf.len = dir.len;
		buf.cat_f("\\%s", CONF_PORTABLE);
		if (fffile_exists(buf.sz()))
			rc = INST_PORTABLE;

	end:
		if (core)
			phi_core_destroy();
		if (phi_dll != FFDL_NULL)
			ffdl_close(phi_dll);
		return rc;
	}

	static int job(int r, const char *path) { return r; }

	/** Delete or restore the backup files. */
	int upgrade_fin(xxvec &buf, xxstr backup_files, bool success) {
		int r = 0;
		const char *s = backup_files.ptr;
		while (s < ffstr_end(&backup_files)) {
			if (success) {
				r |= fffile_remove(s);
			} else {
				buf.clear().add(s).len -= FFS_LEN(".old");
				r |= fffile_rename(s, buf.sz());
			}
			s += ffsz_len(s) + 1;
		}
		return r;
	}

	char* uninstaller_create(xxvec &buf, xxstr dir)
	{
		if (!this->hres_uninst_exe
			&& !(this->hres_uninst_exe = ffui_res_load(GetModuleHandleW(NULL), RES_UNINST, RT_RCDATA, &this->uninst_exe)))
			return ffsz_dup(E_CORRUPT);

		buf.clear().realloc<char>(dir.len + 1 + 255 + 1);
		buf.cat_f("%S\\shell", &dir);
		if (ffdir_make(buf.sz())
			&& !fferr_exist(fferr_last()))
			return ffsz_allocfmt_syserr("directory make: %s", buf.sz());

		buf.len = dir.len;
		buf.cat_f("\\%s", UNINSTALL_EXE);
		if (fffile_writewhole(buf.sz(), this->uninst_exe.ptr, this->uninst_exe.len, 0))
			return ffsz_allocfmt_syserr("file write: %s", buf.sz());
		return NULL;
	}

	void install()
	{
		if (this->done) return;

		xxvec e, dir, exe, buf, backup_files;
		xxstr dir_path, dir_name;
		bool upgrading = 0, dirty = 0, success_msg = 1, was_portable = 0;
		buf.alloc<char>(4096);
		dir.acquire(wmain.edir.text()); // ffui_textstr() writes NULL-terminated string
		exe.add_f("%S\\%s%Z", &dir, EXE_NAME).len--;
		const char *exez = (char*)exe.ptr, *dirz = (char*)dir.ptr;
		ffpath_splitpath_str(dir.str(), &dir_path, &dir_name);
		/*
		exe = path/phiola-2/phiola-gui.exe
		dir = path/phiola-2
		dir_path = path
		*/

		if (!dir_name.equals(DIR_NAME)) {
			e.add_f("%s: \"%S\"", E_DIR_NAME, &dir_name);
			goto err;
		}
		if (!ffpath_abs(dir_path.ptr, dir_path.len)) {
			e.add_f("%s", E_ABS_PATH);
			goto err;
		}
		if (!fffile_exists(buf.add(dir_path).sz())) {
			e.add_f("%s: \"%s\"", E_NO_PATH, buf.sz());
			goto err;
		}

		switch (upgrade_check(buf, dir.str(), NULL)) {
		case INST_NONE:
			break;

		case INST_UPGRADE:
		case INST_UPGRADE_SAME:
			if (wmain.cb_portable.checked()) {
				// The situation has changed since the last UI update
				ui_update();
				return;
			}
			upgrading = 1;
			break;

		case INST_PORTABLE:
			if (!wmain.cb_portable.checked()) {
				// The situation has changed since the last UI update
				ui_update();
				return;
			}
			upgrading = 1;
			was_portable = 1;
			break;

		default:
			// The situation has changed since the last UI update
			ui_update();
			return;
		}

		if (!this->hpkg
			&& !(this->hpkg = ffui_res_load(GetModuleHandleW(NULL), RES_PKG, RT_RCDATA, &this->pkg))) {
			e.add(E_CORRUPT);
			goto err;
		}

		dirty = 1;
		char *s;
		if ((s = zip_unpack(this->pkg, dir_path, upgrading, &backup_files))) {
			if (s != (char*)-1)
				e.acquire(s);
			else
				e.add(E_CORRUPT);
			goto err;
		}

		if (wmain.cb_portable.checked()) {
			if (!was_portable) {
				buf.clear().add_f("%S\\%s%Z", &dir, CONF_PORTABLE);
				fffd f = fffile_open(buf.sz(), FFFILE_CREATENEW | FFFILE_WRITEONLY);
				if (f == FFFILE_NULL)
					e.add_f("Could not create file: %s. ", buf.sz());
				fffile_close(f);
			}
			goto done;
		}

		if ((s = uninstaller_create(buf, dir.str()))) {
			// Note: uninstaller may be left corrupted
			e.acquire(s);
			goto err;
		}

		if (wmain.cb_environ.checked()) {
			if (env_path_add(dir.str()))
				e.add("Could not add phiola into PATH. ");
			else
				ffenv_update();
		}

		if (wmain.cb_shortcut.checked()) {
			if (ffenv_expand(NULL, (char*)buf.ptr, buf.cap, LINK_NAME))
				ffui_createlink(exez, (char*)buf.ptr);
			if (ffenv_expand(NULL, (char*)buf.ptr, buf.cap, START_MENU_LINK))
				ffui_createlink(exez, (char*)buf.ptr);
		}

		// Write phiola path to Registry to allow the user to upgrade without browsing for the path next time
		if (ffwinreg_open_writestr(HKEY_CURRENT_USER, "Software\\phiola", "", dirz, dir.len))
			e.add("Registry write error. ");

		if (shell_ext_reg("phiola", EXE_NAME
			, "Open with phiola"
			, buf.clear().add_f("\"%S\" \"%%1\"%Z", &exe).sz()
			, "enqueue", "Enqueue in phiola"
			, xxvec().add_f("\"%S\" -add \"%%1\"%Z", &exe).sz()
			, (char*)phi_exts, sizeof(phi_exts[0]), FF_COUNT(phi_exts)))
			e.add("Error registering file associations. ");

		if (wmain.cb_start.checked()) {
			ffui_exec(exez);
			success_msg = 0;
		}

		if (wmain.cb_defaultapp.checked())
			ffui_exec("ms-settings:defaultapps");

	done:
		if (upgrading && upgrade_fin(buf, backup_files.str(), 1))
			e.add("Some files from the previous installation could not be removed. ");

		if (e.len)
			ffui_msgdlg_showz(MSG_TITLE, xxvec().add_f("Installation completed with errors: %S%Z", &e).sz(), FFUI_MSGDLG_WARN);
		else if (success_msg)
			ffui_msgdlg_showz(MSG_TITLE, (!upgrading) ? "Installation successful!" : "Upgrade successful!", FFUI_MSGDLG_INFO);

		ffui_post_quitloop();
		this->done = 1;
		return;

	err:
		if (dirty
			&& !upgrading
			&& fffile_exists(dirz)
			&& dir_remove_r(dirz, DEL_MAX_FILES, job))
			e.add_f("%s was not deleted. ", dirz);
		if (upgrading && upgrade_fin(buf, backup_files.str(), 0))
			ffui_msgdlg_showz(MSG_TITLE, "Some files from the previous installation could not be restored.", FFUI_MSGDLG_ERR);
		ffui_msgdlg_showz(MSG_TITLE, e.strz(), FFUI_MSGDLG_ERR);
	}

	void browse() {
		xxvec buf(wmain.edir.text());
		xxstr dir = xxpath(buf.str()).path();
		if (dir.len)
			dir.ptr[dir.len] = '\0';
		xxptr path(ffui_filedlg_show(wmain.wnd.h, (dir.len) ? dir.ptr : NULL, 0));
		if (!path.ptr)
			return;

		wmain.edir.text(xxvec().add_f("%s\\%s%Z", path.ptr, DIR_NAME).sz());
		ui_update();
	}

	void ui_update() {
		xxvec buf, version;
		int r = upgrade_check(buf, xxvec(wmain.edir.text()).str(), &version);
		bool ok = 1;
		const char *action = "Install", *status = "The directory will be created";
		switch (r) {
		case INST_NONE:
			break;

		case INST_UPGRADE:
			action = "Upgrade";
			status = buf.clear().add_f("phiola v%S will be upgraded%Z", &version).sz();
			break;

		case INST_UPGRADE_SAME:
			action = "Reinstall";
			status = buf.clear().add_f("phiola v%S is already installed%Z", &version).sz();
			break;

		case INST_EXISTS:
			status = E_EXISTS;
			ok = 0;
			break;

		case INST_PORTABLE:
			action = "Upgrade";
			if (wmain.cb_portable.checked()) {
				status = buf.clear().add_f("phiola v%S (portable) will be upgraded%Z", &version).sz();
			} else {
				status = "Cannot convert a portable installation to a regular one";
				ok = 0;
			}
			break;
		}

		if ((r == INST_UPGRADE || r == INST_UPGRADE_SAME)
			&& wmain.cb_portable.checked()) {
			status = "Cannot convert an existing installation to portable. Uninstall it first.";
			ok = 0;
		}

		wmain.lstatus.text(status);
		wmain.binstall.text(action);
		wmain.binstall.enable(ok);
	}

	static void main_on_action(ffui_window *wnd, int id)
	{
		struct wmain *wmain = FF_CONTAINER(struct wmain, wnd, wnd);
		installer *g = FF_CONTAINER(installer, wmain, wmain);

		switch (id) {
		case A_DIR_CHANGED:
			g->ui_update();  break;

		case A_BROWSE:
			g->browse();  break;

		case A_INSTALL:
			g->install();  break;

		case A_HOMEPAGE:
			ffui_exec(HOMEPAGE_URL);  break;

		case A_CLOSE:
			ffui_post_quitloop();  break;

		case A_CB_PORTABLE: {
			bool portable = g->wmain.cb_portable.checked();
			g->wmain.cb_shortcut.enable(!portable);
			g->wmain.cb_environ.enable(!portable);
			g->wmain.cb_start.enable(!portable);
			g->wmain.cb_defaultapp.enable(!portable);
			if (portable) {
				g->wmain.cb_shortcut.check(0);
				g->wmain.cb_environ.check(0);
				g->wmain.cb_start.check(0);
				g->wmain.cb_defaultapp.check(0);
			}
			g->ui_update();
			break;
		}
		}
	}

	int load()
	{
		ffui_loader ldr;
		ffui_ldr_init(&ldr, gui_ctl_find, gui_cmd_find, this);
		ldr.hmod_resource = GetModuleHandleW(NULL);

		if (!dark_theme_init(&theme, 0)
			&& DARK_THEME_DARK == dark_theme_query()) {
			dark_theme_colors(&theme, 0x222222, 0xcc99ff);
			dark_theme_ctl(&theme, DARK_THEME_APP, 0);
			ldr.dark_theme = 1;
			ffui_theme = &theme;
		}

		HGLOBAL hres;
		ffstr ui;
		if (!(hres = ffui_res_load(ldr.hmod_resource, RES_UI, RT_RCDATA, &ui)))
			return -1;

		ffui_ldr_source(&ldr, FFSTR_Z(""), ui);
		if (ffui_ldr_load(&ldr, NULL)) {
			return -1;
		}

		wmain.wnd.top = 1;
		wmain.wnd.on_action = main_on_action;
		wmain.wnd.onclose_id = A_CLOSE;
		ffui_thd_post(show, this);

		ffui_ldr_fin(&ldr);
		ffui_res_close(hres);
		return 0;
	}

	static void show(void *param)
	{
		installer *g = (installer*)param;
		xxvec buf;
		xxstr s;
		if (!ffwinreg_open_readstr(HKEY_CURRENT_USER, "Software\\phiola", "", &s))
			buf.acquire(s);
		else
			s = buf.acquire(ffenv_expand(NULL, NULL, 0, DEFAULT_INSTALL_PATH)).str();
		g->wmain.edir.text(s);

		g->wmain.lurl.text(HOMEPAGE_URL);
		g->wmain.wnd.title(TITLE);
		g->wmain.wnd.show(1);
		g->ui_update();
	}
};

#ifdef FF_DEBUG
int main()
#else
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#endif
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	installer *g = ffmem_new(installer);
	ffui_init();
	if (g->load()) return 1;
	ffui_run();
#ifdef FF_DEBUG
	g->~installer();
	ffmem_free(g);
#endif
	return 0;
}
