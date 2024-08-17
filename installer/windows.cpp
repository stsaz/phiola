/** phiola/Windows installer
Simon Zolin, 2024 */

#define DIR_NAME  "phiola-2"
#define EXE_NAME  "phiola-gui.exe"
#define DEFAULT_INSTALL_PATH  "%USERPROFILE%\\" DIR_NAME
#define LINK_NAME  "%USERPROFILE%\\Desktop\\phiola.lnk"
#define TITLE  "Set up phiola v" PHI_VERSION_STR
#define HOMEPAGE_URL  "https://github.com/stsaz/phiola"
#define RES_UI  MAKEINTRESOURCEA(1)
#define RES_PKG  MAKEINTRESOURCEA(2)

#define MSG_TITLE  "phiola setup"
#define E_EXISTS  "The specified directory already exists"
#define E_NO_PATH  "The specified path does not exist"
#define E_ABS_PATH  "The install path must be absolute"
#define E_DIR_NAME  "The directory name must be \"phiola-2\", but you specified"
#define E_CORRUPT  "The installer file is corrupted.  Please redownload it."

typedef long long int64;
typedef unsigned long long uint64;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char u_char;

#include <util/windows-shell.h>
#ifdef FF_DEBUG
#include <ffsys/std.h>
#endif
#include <util/util.hpp>
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
	_(A_CLOSE),

#define _(id) id
enum {
	A_NONE,
	INCLUDE_ACTIONS(_)
};
#undef _

struct installer {
	struct wmain {
		ffui_windowxx	wnd;
		ffui_labelxx	ldir, lurl;
		ffui_editxx		edir;
		ffui_checkboxxx	cbshortcut, cbenviron, cbstart;
		ffui_buttonxx	bbrowse, binstall;
		ffui_image		ico;
	} wmain;

	HGLOBAL	hpkg;
	ffstr	pkg;
	uint	done;

	~installer()
	{
		ffui_res_close(hpkg);
	}

	static void* gui_ctl_find(void *udata, const ffstr *name)
	{
		#define _(m) FFUI_LDR_CTL(struct wmain, m)
		static const ffui_ldr_ctl wmain_ctls[] = {
			_(wnd),
			_(ldir), _(edir), _(bbrowse),
			_(cbshortcut),
			_(cbenviron),
			_(cbstart),
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
				return i + 1;
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

		exe.add_f("%S\\%s%Z", &dir, EXE_NAME);

		{
		unsigned f_shortcut = wmain.cbshortcut.checked(),
			f_env = wmain.cbenviron.checked();
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

		if (wmain.cbstart.checked())
			ffui_exec(exe.sz());

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
		}
	}

	int load()
	{
		ffui_loader ldr;
		ffui_ldr_init(&ldr, gui_ctl_find, gui_cmd_find, this);
		ldr.hmod_resource = GetModuleHandleW(NULL);

		HGLOBAL hres;
		ffstr ui;
		if (!(hres = ffui_res_load(ldr.hmod_resource, RES_UI, RT_RCDATA, &ui)))
			return -1;

#ifdef FF_DEBUG
		if (ffui_ldr_loadfile(&ldr, "../installer/windows.ui")) {
			fflog("parsing ui: %s", ffui_ldr_errstr(&ldr));
			return -1;
		}
#else
		if (ffui_ldr_load(&ldr, ui)) {
			return -1;
		}
#endif

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
