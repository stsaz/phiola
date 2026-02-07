/** phiola: GUI: rename file
2025, Simon Zolin */

struct gui_wrename {
	ffui_windowxx	wnd;
	ffui_editxx		turl;
	ffui_buttonxx	brename;

	struct phi_queue_entry *qe;
};

FF_EXTERN const ffui_ldr_ctl wrename_ctls[] = {
	FFUI_LDR_CTL(gui_wrename, wnd),
	FFUI_LDR_CTL(gui_wrename, turl),
	FFUI_LDR_CTL(gui_wrename, brename),
	FFUI_LDR_CTL_END
};

static void wrename_action(ffui_window *wnd, int id)
{
	gui_wrename *w = gg->wrename;

	switch (id) {
	case A_RENAME_RENAME: {
		xxvec s = w->turl.text();
		if (!s.len)
			break;

		gd->qe_rename = w->qe,  w->qe = NULL;
		gui_core_task_ptr(file_rename, s.ptr);
		s.reset();
		w->wnd.show(0);
		break;
	}
	}
}

void wrename_show(uint show, uint idx)
{
	gui_wrename *w = gg->wrename;
	if (gui_dlg_load())
		return;

	if (w->qe)
		gd->queue->unref(w->qe);
	if (!(w->qe = list_vis_qe_ref(idx)))
		return;

	w->turl.text(w->qe->url);
	w->turl.focus();
	w->wnd.show(1);
	w->wnd.present();

	ffstr name = xxpath(w->qe->url).name_no_ext();
	uint off = name.ptr - w->qe->url;
	w->turl.select(off, off + name.len);
}

void wrename_init()
{
	gui_wrename *w = gui_allocT(gui_wrename);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = wrename_action;
	gg->wrename = w;
}

void wrename_fin()
{
	gui_wrename *w = gg->wrename;
	if (w->qe)
		gd->queue->unref(w->qe);
}
