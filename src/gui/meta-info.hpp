/** phiola: GUI: display file meta info
2023, Simon Zolin */

struct gui_winfo {
	ffui_wndxx wnd;
	ffui_viewxx vinfo;
};

FF_EXTERN const ffui_ldr_ctl winfo_ctls[] = {
	FFUI_LDR_CTL(gui_winfo, wnd),
	FFUI_LDR_CTL(gui_winfo, vinfo),
	FFUI_LDR_CTL_END
};

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

void winfo_show(uint show, uint idx)
{
	gui_winfo *w = gg->winfo;

	if (!show) {
		w->wnd.show(0);
		return;
	}

	struct phi_queue_entry *qe = gd->queue->ref(list_id_visible(), idx);
	if (qe == NULL) return;

	w->wnd.title(qe->conf.ifile.name);
	winfo_display(qe);
	gd->queue->unref(qe);

	w->wnd.show(1);
}

static void winfo_action(ffui_wnd *wnd, int id)
{
}

void winfo_init()
{
	gui_winfo *w = ffmem_new(gui_winfo);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = winfo_action;
	gg->winfo = w;
}
