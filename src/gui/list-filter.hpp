/** phiola: GUI: filter tracks
2023, Simon Zolin */

struct gui_wlistfilter {
	ffui_windowxx wnd;
	ffui_editxx tfilter;
	uint initialized :1;
};

FF_EXTERN const ffui_ldr_ctl wlistfilter_ctls[] = {
	FFUI_LDR_CTL(gui_wlistfilter, wnd),
	FFUI_LDR_CTL(gui_wlistfilter, tfilter),
	FFUI_LDR_CTL_END
};

static void wlistfilter_action(ffui_window *wnd, int id)
{
	gui_wlistfilter *w = gg->wlistfilter;

	switch (id) {
	case A_LISTFILTER_SET:
		gui_core_task_data(list_filter, w->tfilter.text());  break;

	case A_LISTFILTER_CLOSE:
		gui_core_task_data(list_filter, xxstr());  break;
	}
}

void wlistfilter_show()
{
	gui_wlistfilter *w = gg->wlistfilter;
	w->initialized = 1;
	w->tfilter.focus();
	w->wnd.show(1);
	w->wnd.present();
}

static void wlistfilter_init()
{
	gui_wlistfilter *w = gui_allocT(gui_wlistfilter);
	w->wnd.on_action = wlistfilter_action;
	w->wnd.hide_on_close = 1;
	gg->wlistfilter = w;
}

void wlistfilter_cmd(uint cmd)
{
	gui_wlistfilter *w = gg->wlistfilter;
	switch (cmd) {
	case GUI_CMD_INIT:
		wlistfilter_init();  break;
	case GUI_CMD_HIDE:
		if (w->initialized)
			w->wnd.show(0);
		break;
	}
}
