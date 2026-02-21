/** phiola: GUI: display file meta info
2023, Simon Zolin */

#include <ffsys/file.h>
#include <ffbase/lock.h>

struct gui_winfo {
	ffui_windowxx	wnd;
	ffui_menu		mm;
	ffui_viewxx		vinfo;

	xxvec keys; // ffstr[]
	uint changed;
	struct phi_queue_entry *qe;

	uint edit_idx;

	char *wnd_pos;
	uint initialized :1;
};

#define META_N  4

FF_EXTERN const ffui_ldr_ctl winfo_ctls[] = {
	FFUI_LDR_CTL(gui_winfo, wnd),
	FFUI_LDR_CTL(gui_winfo, mm),
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

static void winfo_addpair(xxstr name, xxstr val)
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
	w->keys.reset();
	w->changed = 0;

	winfo_addpair("File path", qe->url);

	fffileinfo fi = {};
	ffbool have_fi = (0 == fffile_info_path(qe->url, &fi));

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

	fflock_lock((fflock*)&qe->lock); // core thread may read or write `conf.meta` at this moment
	const phi_meta *meta = &qe->meta;

	core->metaif->find(meta, FFSTR_Z("_phi_info"), &val, PHI_META_PRIVATE);
	winfo_addpair("Info", val);

	uint i = 0;
	while (core->metaif->list(meta, &i, &name, &val, 0)) {
		winfo_addpair(name, val);
		ffstr_dupstr(w->keys.push_z<ffstr>(), &name);
	}
	fflock_unlock((fflock*)&qe->lock);
}

void winfo_show(uint show, uint idx)
{
	gui_winfo *w = gg->winfo;
	if (gui_dlg_load())
		return;

	if (w->qe)
		gd->queue->unref(w->qe);
	w->qe = NULL;

	if (!show) {
		w->wnd.show(0);
		return;
	}

	struct phi_queue_entry *qe = list_vis_qe_ref(idx);
	if (qe == NULL) return;

	if (!w->initialized) {
		w->initialized = 1;

		if (w->wnd_pos)
			conf_wnd_pos_read(&w->wnd, FFSTR_Z(w->wnd_pos));
		ffmem_free(w->wnd_pos);

		if (gd->conf.tags_keep_date)
			gg->mminfo_file.check(A_INFO_KEEPDATE, 1);
	}

	w->wnd.title(qe->url);
	winfo_display(qe);
	w->qe = qe;
	// keep the entry locked

	w->wnd.show(1);
}

static void winfo_edit(uint idx, const char *new_text)
{
	gui_winfo *w = gg->winfo;
	ffstr val = FFSTR_Z(new_text);
	uint ki = idx - META_N;
	if ((int)ki < 0)
		return;
	ffstr name = *w->keys.at<ffstr>(ki);
	fflock_lock((fflock*)&w->qe->lock); // core thread may read or write `meta` at this moment
	core->metaif->set(&w->qe->meta, name, val, PHI_META_REPLACE);
	fflock_unlock((fflock*)&w->qe->lock);
	if (ki >= 32)
		warnlog("can write only up to 32 tags");
	ffbit_set32(&w->changed, ki);
	w->vinfo.set(idx, 0, xxvec().add_f("%S (*)", &name).str());
	w->vinfo.set(idx, 1, val);
}

static void winfo_tag_add(ffstr name)
{
	gui_winfo *w = gg->winfo;
	if (!w->qe) return;

	ffstr val;
	if (!core->metaif->find(&w->qe->meta, name, &val, 0)) {
		warnlog("tag already exists: %S", &name);
		return;
	}
	val = FFSTR_Z("");
	fflock_lock((fflock*)&w->qe->lock); // core thread may read or write `meta` at this moment
	core->metaif->set(&w->qe->meta, name, val, 0);
	fflock_unlock((fflock*)&w->qe->lock);
	winfo_addpair(name, val);
	ffstr_dupstr(w->keys.push_z<ffstr>(), &name);
	ffbit_set32(&w->changed, w->keys.len - 1);
}

/** Get the list of modified or newly added tags and write them to file */
static void winfo_write()
{
	gui_winfo *w = gg->winfo;
	if (!w->qe) return;

	const phi_tag_if *tag = (phi_tag_if*)core->mod("format.tag");

	xxvec m;
	ffstr k, v;
	uint bits = w->changed, i;
	while (bits) {
		i = ffbit_rfind32(bits) - 1;
		ffbit_reset32(&bits, i);
		k = *w->keys.at<ffstr>(i);
		if (!core->metaif->find(&w->qe->meta, k, &v, 0)) {
			ffvec s = {};
			ffvec_addfmt(&s, "%S=%S", &k, &v);
			*m.push<ffstr>() = *(ffstr*)&s;
		}
	}

	if (!m.len)
		return;

	struct phi_tag_conf conf = {};
	conf.filename = w->qe->url;
	conf.preserve_date = gd->conf.tags_keep_date;
	conf.meta = m.slice();
	if (!tag->edit(&conf)) {
		core->metaif->destroy(&w->qe->meta);
		w->keys.reset();
		w->changed = 0;
		w->vinfo.clear();
		if (w->qe)
			gd->queue->unref(w->qe);
		w->qe = NULL;
	}

	FFSLICE_FOREACH_T(&m, ffstr_free, ffstr);
}

static void winfo_action(ffui_window *wnd, int id)
{
	gui_winfo *w = gg->winfo;
	ffstr name;

	switch (id) {
	case A_INFO_EDIT: {
#ifdef FF_WIN
		int i, isub;
		ffui_point pt;
		ffui_cur_pos(&pt);
		if (-1 == (i = ffui_view_hittest(&w->vinfo, &pt, &isub))
			|| isub != 1)
			break;
		ffui_view_edit(&w->vinfo, i, 1);
		w->edit_idx = i;
#endif
		break;
	}

	case A_INFO_EDIT_DONE:
#ifdef FF_WIN
		winfo_edit(w->edit_idx, w->vinfo.text);
#else
		winfo_edit(w->vinfo.edited.idx, w->vinfo.edited.new_text);
#endif
		break;

	case A_INFO_ADD_ARTIST:
		ffstr_setz(&name, "artist");  goto tag_add;
	case A_INFO_ADD_TITLE:
		ffstr_setz(&name, "title");  goto tag_add;
	tag_add:
		winfo_tag_add(name);
		break;

	case A_INFO_KEEPDATE:
		gg->mminfo_file.check(A_INFO_KEEPDATE, !!(gd->conf.tags_keep_date = !gd->conf.tags_keep_date));  break;

	case A_INFO_WRITE:
		gui_core_task(winfo_write);  break;
	}
}

void winfo_init()
{
	gui_winfo *w = gui_allocT(gui_winfo);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = winfo_action;
#ifdef FF_WIN
	w->vinfo.lclick_id = A_INFO_EDIT;
#endif
	w->vinfo.edit_id = A_INFO_EDIT_DONE;
	gg->winfo = w;
}

void winfo_fin()
{
	gui_winfo *w = gg->winfo;
	FFSLICE_FOREACH_T(&w->keys, ffstr_free, ffstr);
	if (w->qe)
		gd->queue->unref(w->qe);
}
