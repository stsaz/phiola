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
#define E_ABS_PATH  "The install path must be absolute"
#define E_DIR_NAME  "The directory name must be \"phiola-2\", but you specified"
#define E_CORRUPT  "The installer file is corrupted.  Please redownload it."

#include <util/windows-shell.h>
#ifdef FF_DEBUG
#include <ffsys/std.h>
#endif
#include <utils.h>
#include <ffgui/winapi/loader.h>
#include <ffgui/loader.h>
#include <ffgui/gui.hpp>
#include <ffsys/environ.h>
#include <ffsys/globals.h>

#define INCLUDE_ACTIONS(_) \
	_(A_INSTALL), \
	_(A_BROWSE), \
	_(A_HOMEPAGE), \
	_(A_CLOSE), \
	_(A_CB_PORTABLE),

#define _(id) id
enum {
	A_NONE = 999,
	INCLUDE_ACTIONS(_)
};
#undef _

struct installer {
	struct wmain {
		ffui_windowxx	wnd;
		ffui_labelxx	ldir, lurl;
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
			_(ldir), _(edir), _(bbrowse),
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

	void browse()
	{
		ffui_dialogxx dlg;
		dlg.title(MSG_TITLE);
		char *fn = dlg.save(&wmain.wnd, DIR_NAME);
		if (fn)
			wmain.edir.text(fn);
	}

	char* uninstaller_create(xxstr dir)
	{
		if (!this->hres_uninst_exe
			&& !(this->hres_uninst_exe = ffui_res_load(GetModuleHandleW(NULL), RES_UNINST, RT_RCDATA, &this->uninst_exe)))
			return ffsz_dup(E_CORRUPT);

		xxvec buf;
		buf.alloc<char>(dir.len + FFS_LEN(UNINSTALL_EXE) + 2);
		buf.cat_f("%S\\shell", &dir);

		if (ffdir_make(buf.sz())
			&& !fferr_exist(fferr_last()))
			return ffsz_allocfmt_syserr("directory make: %s", buf.sz());

		buf.len = dir.len;
		buf.cat_f("\\%s", UNINSTALL_EXE);
		if (fffile_writewhole(buf.sz(), this->uninst_exe.ptr, this->uninst_exe.len, FFFILE_CREATENEW))
			return ffsz_allocfmt_syserr("file write: %s", buf.sz());
		return NULL;
	}

	void install()
	{
		if (this->done) return;

		xxvec e, dir, path, exe;
		xxstr spath, sname;

		dir.acquire(wmain.edir.text());
		ffvec_grow(&dir, 1, 1);
		((char*)dir.ptr)[dir.len] = '\0';
		ffpath_splitpath_str(dir.str(), &spath, &sname);

		if (!sname.equals(DIR_NAME)) {
			e.add_f("%s: \"%S\"%Z", E_DIR_NAME, &sname);
			goto err;
		}

		if (!ffpath_abs(dir.sz(), dir.len)) {
			e.add_f("%s: \"%s\"%Z", E_ABS_PATH, dir.sz());
			goto err;
		}

		if (fffile_exists(dir.sz())) {
			e.add_f("%s: \"%s\"%Z", E_EXISTS, dir.sz());
			goto err;
		}

		path.add_f("%S%Z", &spath);
		if (!fffile_exists(path.sz())) {
			e.add_f("%s: \"%s\"%Z", E_NO_PATH, path.sz());
			goto err;
		}

		if (!this->hpkg
			&& !(this->hpkg = ffui_res_load(GetModuleHandleW(NULL), RES_PKG, RT_RCDATA, &this->pkg))) {
			e.set(E_CORRUPT);
			goto err;
		}

		char *s;
		if ((s = zip_unpack(this->pkg, spath))) {
			if (s != (char*)-1)
				e.acquire(s);
			else
				e.set(E_CORRUPT);
			goto err;
		}

		if (wmain.cb_portable.checked()) {
			fffd f = fffile_open(xxvec().add_f("%S\\%s%Z", &dir, CONF_PORTABLE).sz(), FFFILE_CREATENEW | FFFILE_WRITEONLY);
			fffile_close(f);
			goto done;
		}

		if ((s = uninstaller_create(dir.str()))) {
			e.acquire(s);
			goto err;
		}

		exe.add_f("%S\\%s%Z", &dir, EXE_NAME);

		{
		unsigned f_shortcut = wmain.cb_shortcut.checked(),
			f_env = wmain.cb_environ.checked();
		if (f_shortcut || f_env)
			CoInitializeEx(NULL, 0);

		if (f_env) {
			if (!env_path_add(dir.str()))
				ffenv_update();
		}

		if (f_shortcut) {
			xxvec desktop(ffenv_expand(NULL, NULL, 0, LINK_NAME));
			ffui_createlink(exe.sz(), desktop.sz());
		}
		}

		shell_ext_reg(EXE_NAME
			, "Open with phiola"
			, xxvec().add_f("\"%s\" \"%%1\"%Z", exe.sz()).sz()
			, "Enqueue in phiola"
			, xxvec().add_f("\"%s\" -add \"%%1\"%Z", exe.sz()).sz()
			, (char*)phi_exts, sizeof(phi_exts[0]), FF_COUNT(phi_exts));

		if (wmain.cb_start.checked())
			ffui_exec(exe.sz());

		if (wmain.cb_defaultapp.checked())
			ffui_exec("ms-settings:defaultapps");

	done:
		ffui_post_quitloop();
		this->done = 1;
		return;

	err:
		ffui_msgdlg_showz(MSG_TITLE, e.sz(), FFUI_MSGDLG_ERR);
	}

	static void main_on_action(ffui_window *wnd, int id)
	{
		struct wmain *wmain = FF_STRUCTPTR(struct wmain, wnd, wnd);
		installer *g = FF_STRUCTPTR(installer, wmain, wmain);

		switch (id) {
		case A_BROWSE:
			g->browse();  break;

		case A_INSTALL:
			g->install();  break;

		case A_HOMEPAGE:
			ffui_exec(HOMEPAGE_URL);  break;

		case A_CLOSE:
			ffui_post_quitloop();  break;

		case A_CB_PORTABLE: {
			uint portable = g->wmain.cb_portable.checked();
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
		g->wmain.edir.text(xxvec(ffenv_expand(NULL, NULL, 0, DEFAULT_INSTALL_PATH)).str());
		g->wmain.lurl.text(HOMEPAGE_URL);
		g->wmain.wnd.title(TITLE);
		g->wmain.wnd.show(1);
	}
};

#ifdef FF_DEBUG
int main()
#else
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#endif
{
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
