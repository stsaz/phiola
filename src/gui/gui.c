/** phiola: GUI: loader & worker
2023, Simon Zolin */

#include <gui/gui.h>
#include <util/conf-args.h>
#include <ffsys/file.h>
#include <ffsys/perf.h>

struct gui *gg;
static void theme_apply(const char *theme);

void gui_task_ptr(void (*func)(void*), void *ptr)
{
	ffui_thd_post(func, ptr);
}

#ifdef FF_WIN
static int _fdrop_next(ffvec *fn, ffstr *dropdata)
{
	if (!dropdata->len) return -1;

	ffstr line;
	ffstr_splitby(dropdata, '\n', &line, dropdata);
	ffslice_set2(fn, &line);
	return 0;
}
#endif

void gui_dragdrop(ffstr data)
{
	ffvec fn = {};
	ffstr d = data;
#ifdef FF_WIN
	while (!_fdrop_next(&fn, &d)) {
#else
	while (!ffui_fdrop_next(&fn, &d)) {
#endif
		list_add(*(ffstr*)&fn);
	}
	ffvec_free(&fn);
	ffstr_free(&data);
}

void conf_wnd_pos_read(ffui_window *w, ffstr val)
{
	ffui_pos pos;
	if (!ffstr_matchfmt(&val, "%d %d %u %u", &pos.x, &pos.y, &pos.cx, &pos.cy))
		ffui_post_wnd_place(w, SW_SHOWNORMAL, &pos);
}

extern const struct ffarg guimod_args[];

static struct ffarg_ctx winfo_args_f() {
	struct ffarg_ctx ax = { winfo_args, gg->winfo };
	return ax;
}

static struct ffarg_ctx wrecord_args_f() {
	struct ffarg_ctx ax = { wrecord_args, gg->wrecord };
	return ax;
}

static struct ffarg_ctx wconvert_args_f() {
	struct ffarg_ctx ax = { wconvert_args, gg->wconvert };
	return ax;
}

static struct ffarg_ctx wsettings_args_f() {
	struct ffarg_ctx ax = { wsettings_args, gg->wsettings };
	return ax;
}

static struct ffarg_ctx wmain_args_f() {
	struct ffarg_ctx ax = { wmain_args, gg->wmain };
	return ax;
}

static struct ffarg_ctx guimod_args_f() {
	struct ffarg_ctx ax = { guimod_args, gd };
	return ax;
}

static struct ffarg_ctx wlog_args_f() {
	struct ffarg_ctx ax = { wlog_args, gg->wlog };
	return ax;
}

static const struct ffarg args[] = {
	{ "convert",	'{',	wconvert_args_f },
	{ "info",		'{',	winfo_args_f },
	{ "log",		'{',	wlog_args_f },
	{ "main",		'{',	wmain_args_f },
	{ "mod",		'{',	guimod_args_f },
	{ "record",		'{',	wrecord_args_f },
	{ "settings",	'{',	wsettings_args_f },
	{}
};

void gui_userconf_load()
{
	ffvec buf = {};
	if (fffile_readwhole(gd->user_conf_name, &buf, 1*1024*1024))
		goto end;

	struct ffargs a = {};
	ffstr d = FFSTR_INITSTR(&buf);
	int r = ffargs_process_conf(&a, args, NULL, 0, d);
	if (r)
		warnlog("%s:%s", gd->user_conf_name, a.error);

end:
	ffvec_free(&buf);
}

static void gui_userconf_save()
{
	ffconfw cw;
	uint flags = FFCONFW_FINDENT | FFCONFW_FKVTAB;
#ifdef FF_WIN
	flags |= FFCONFW_FCRLF;
#endif
	ffconfw_init(&cw, flags);

	ffconfw_add2obj(&cw, "mod", '{');
		mod_userconf_write(&cw);
	ffconfw_add_obj(&cw, '}');

	ffconfw_add2obj(&cw, "main", '{');
		wmain_userconf_write(&cw);
	ffconfw_add_obj(&cw, '}');

	ffconfw_add2obj(&cw, "record", '{');
		wrecord_userconf_write(&cw);
	ffconfw_add_obj(&cw, '}');

	ffconfw_add2obj(&cw, "convert", '{');
		wconvert_userconf_write(&cw);
	ffconfw_add_obj(&cw, '}');

	ffconfw_add2obj(&cw, "info", '{');
		winfo_userconf_write(&cw);
	ffconfw_add_obj(&cw, '}');

	ffconfw_add2obj(&cw, "settings", '{');
		wsettings_userconf_write(&cw);
	ffconfw_add_obj(&cw, '}');

	ffconfw_add2obj(&cw, "log", '{');
		wlog_userconf_write(&cw);
	ffconfw_add_obj(&cw, '}');

	ffconfw_fin(&cw);

	userconf_save(*(ffstr*)&cw.buf);
}

extern const ffui_ldr_ctl
	wmain_ctls[],
	winfo_ctls[],
	wrename_ctls[],
	wsettings_ctls[],
	wgoto_ctls[],
	wlistadd_ctls[],
	wlistfilter_ctls[],
	wrecord_ctls[],
	wconvert_ctls[],
	wabout_ctls[],
	wlog_ctls[];

static void* gui_getctl(void *udata, const ffstr *name)
{
	#define _(m)  FFUI_LDR_CTL(struct gui, m)
	#define _w(w, ctls)  FFUI_LDR_CTL3_PTR(struct gui, w, ctls)
	static const ffui_ldr_ctl top_ctls[] = {
		_(mfile),
		_(mlist),
		_(mplay),
		_(mrecord),
		_(mconvert),
		_(mhelp),
		_(mpopup),
		_(dlg),
		_(mminfo_file), _(mminfo_addtag),
		_w(wmain, wmain_ctls),
		_w(winfo, winfo_ctls),
		_w(wrename, wrename_ctls),
		_w(wsettings, wsettings_ctls),
		_w(wgoto, wgoto_ctls),
		_w(wlistfilter, wlistfilter_ctls),
		_w(wlistadd, wlistadd_ctls),
		_w(wrecord, wrecord_ctls),
		_w(wconvert, wconvert_ctls),
		_w(wabout, wabout_ctls),
		_w(wlog, wlog_ctls),
		FFUI_LDR_CTL_END
	};
	#undef _
	#undef _w

	struct gui *g = udata;
	return ffui_ldr_findctl(top_ctls, g, name);
}

static int gui_getcmd(void *udata, const ffstr *name)
{
	static const char action_str[][24] = {
		#define X(id)  #id
		#include "actions.h"
		#undef X
	};

	for (uint i = 0;  i != FF_COUNT(action_str);  i++) {
		if (ffstr_eqz(name, action_str[i]))
			return i + 1;
	}
	return 0;
}

static int load_ui()
{
	int r = -1;
	char *fn = ffsz_allocfmt("%Smod/gui/ui.conf", &core->conf.root);
	ffui_loader ldr;
	ffui_ldr_init(&ldr, gui_getctl, gui_getcmd, gg);
	ffmem_copy(ldr.language, core->conf.language, sizeof(ldr.language));
#ifdef FF_WIN
	ldr.hmod_resource = GetModuleHandleW(L"gui.dll");
	ldr.dark_mode = (gd->conf.theme && ffsz_eq(gd->conf.theme, "dark"));
#endif

	fftime t1;
	if (core->conf.log_level >= PHI_LOG_DEBUG)
		t1 = core->time(NULL, PHI_CORE_TIME_MONOTONIC);

	if (ffui_ldr_loadfile(&ldr, fn)) {
		errlog("parsing ui.conf: %s", ffui_ldr_errstr(&ldr));
		goto done;
	}

	theme_apply(gd->conf.theme);
	r = 0;

done:
	if (core->conf.log_level >= PHI_LOG_DEBUG) {
		fftime t2 = core->time(NULL, PHI_CORE_TIME_MONOTONIC);
		fftime_sub(&t2, &t1);
		dbglog("loaded GUI in %Ums", (int64)fftime_to_msec(&t2));
	}

	ffui_ldr_fin(&ldr);
	ffmem_free(fn);
	return r;
}

static void theme_apply(const char *theme)
{
	if (!theme) return;

	dbglog("applying theme %s", theme);

	ffui_loader ldr;
	ffui_ldr_init(&ldr, gui_getctl, NULL, gg);

	char *fn = ffsz_allocfmt("%Smod/gui/theme-%s.conf"
		, &core->conf.root, theme);
	ffvec buf = {};
	if (fffile_readwhole(fn, &buf, 1*1024*1024)) {
		syserrlog("file read: %s", fn);
		goto end;
	}

	ffui_ldr_loadconf(&ldr, *(ffstr*)&buf);

end:
	ffmem_free(fn);
	ffvec_free(&buf);
	ffui_ldr_fin(&ldr);
}

void theme_switch(uint i)
{
	if (i) {
		gd->conf.theme = ffsz_dup("dark");
		theme_apply(gd->conf.theme);
	} else {
		ffmem_free(gd->conf.theme);
		gd->conf.theme = NULL;
	}
}

void gui_init()
{
	FF_ASSERT(sizeof(struct gui) <= 4096);
	uint n = 4096;
	gg = ffmem_align(n, 4096);
	ffmem_zero(gg, n);
	memarea_init(&gg->area, n - sizeof(struct gui));

	wmain_init();
	winfo_init();
	wrename_init();
	wsettings_init();
	wgoto_init();
	wlistadd_init();
	wlistfilter_init();
	wrecord_init();
	wconvert_init();
	wabout_init();
	wlog_init();
}

void* gui_alloc(uint n)
{
	dbglog("alloc %u bytes [%u]", n, gg->area.size);
	return memarea_alloc(&gg->area, n);
}

extern phi_log_ctl gui_log_ctl;

int FFTHREAD_PROCCALL gui_worker(void *param)
{
	ffui_init();
	if (load_ui())
		goto end;
	if (gui_log_ctl)
		gui_log_ctl(1);

	ffui_thd_post((void(*)(void*))wmain_show, NULL);

	dbglog("entering GUI loop");
	ffui_run();
	dbglog("exited GUI loop");
	if (gui_log_ctl)
		gui_log_ctl(0);

end:
	ffui_uninit();
	gui_core_task_uint(gui_stop, 0);
	return 0;
}

void gui_quit()
{
	gui_userconf_save();
	winfo_fin();
	wrename_fin();
	wmain_fin();
	ffui_post_quitloop();
}
