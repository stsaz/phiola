/** phiola: GUI: display file meta info
2023, Simon Zolin */

struct gui_winfo {
	ffui_windowxx wnd;
	ffui_viewxx vinfo;

	char *wnd_pos;
	uint initialized :1;
};

FF_EXTERN const ffui_ldr_ctl winfo_ctls[] = {
	FFUI_LDR_CTL(gui_winfo, wnd),
	FFUI_LDR_CTL(gui_winfo, vinfo),
	FFUI_LDR_CTL_END
};

#define O(m)  (void*)FF_OFF(gui_winfo, m)
const ffarg winfo_args[] = {
	{ "winfo.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

void winfo_userconf_write(ffconfw *cw)
{
	gui_winfo *w = gg->winfo;
	if (w->initialized)
		conf_wnd_pos_write(cw, "winfo.pos", &w->wnd);
	else if (w->wnd_pos)
		ffconfw_add2z(cw, "winfo.pos", w->wnd_pos);
}

static void winfo_addpair(ffstrxx name, ffstrxx val)
{
	gui_winfo *w = gg->winfo;
	int i = w->vinfo.append(name);
	w->vinfo.set(i, 1, val);
}

static void winfo_display(struct phi_queue_entry *qe)
{
	gui_winfo *w = gg->winfo;
	char buf[255];
	ffstr data = {}, name, val = {};
	data.ptr = buf;

	w->vinfo.clear();

	winfo_addpair("File path", qe->conf.ifile.name);

	fffileinfo fi = {};
	ffbool have_fi = (0 == fffile_info_path(qe->conf.ifile.name, &fi));

	data.len = 0;
	if (have_fi)
		ffstr_addfmt(&data, sizeof(buf), "%U KB", fffileinfo_size(&fi) / 1024);
	winfo_addpair("File size", data);

	data.len = 0;
	if (have_fi) {
		fftime t = fffileinfo_mtime(&fi);
		t.sec += FFTIME_1970_SECONDS; // UTC
		ffdatetime dt;
		fftime_split1(&dt, &t);
		data.len = fftime_tostr1(&dt, data.ptr, sizeof(buf), FFTIME_DATE_WDMY | FFTIME_HMS);
	}
	winfo_addpair("File date", data);

	gd->metaif->find(&qe->conf.meta, FFSTR_Z("_phi_info"), &val, PHI_META_PRIVATE);
	winfo_addpair("Info", val);

	uint i = 0;
	while (gd->metaif->list(&qe->conf.meta, &i, &name, &val, 0)) {
		winfo_addpair(name, val);
	}
}

static void winfo_edit()
{
#ifdef FF_WIN
	gui_winfo *w = gg->winfo;
	int i, isub;
	ffui_point pt;
	ffui_cur_pos(&pt);
	if (-1 == (i = ffui_view_hittest(&w->vinfo, &pt, &isub))
		|| isub != 1)
		return;
	ffui_view_edit(&w->vinfo, i, 1);
#endif
}

void winfo_show(uint show, uint idx)
{
	gui_winfo *w = gg->winfo;

	if (!show) {
		w->wnd.show(0);
		return;
	}

	struct phi_queue_entry *qe = gd->queue->ref(list_id_visible(), idx);
	if (qe == NULL) return;

	if (!w->initialized) {
		w->initialized = 1;

		if (w->wnd_pos)
			conf_wnd_pos_read(&w->wnd, FFSTR_Z(w->wnd_pos));
		ffmem_free(w->wnd_pos);
	}

	w->wnd.title(qe->conf.ifile.name);
	winfo_display(qe);
	gd->queue->unref(qe);

	w->wnd.show(1);
}

static void winfo_action(ffui_window *wnd, int id)
{
	switch (id) {
	case A_INFO_EDIT:
		winfo_edit();  break;
	}
}

void winfo_init()
{
	gui_winfo *w = ffmem_new(gui_winfo);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = winfo_action;
	gg->winfo = w;
}
