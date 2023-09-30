/** phiola: GUI: filter tracks
2023, Simon Zolin */

struct gui_wlistfilter {
	ffui_windowxx wnd;
	ffui_editxx tfilter;
};

FF_EXTERN const ffui_ldr_ctl wlistfilter_ctls[] = {
	FFUI_LDR_CTL(gui_wlistfilter, wnd),
	FFUI_LDR_CTL(gui_wlistfilter, tfilter),
	FFUI_LDR_CTL_END
};

static void wlistfilter_action(ffui_wnd *wnd, int id)
{
	gui_wlistfilter *w = gg->wlistfilter;

	switch (id) {
	case A_LISTFILTER_SET:
		gui_core_task_data(list_filter, w->tfilter.text());  break;

	case A_LISTFILTER_CLOSE:
		gui_core_task_data(list_filter, ffstrxx());  break;
	}
}

void wlistfilter_show(uint show)
{
	gui_wlistfilter *w = gg->wlistfilter;
	if (show)
		w->tfilter.focus();
	w->wnd.show(show);
}

void wlistfilter_init()
{
	gui_wlistfilter *w = ffmem_new(gui_wlistfilter);
	w->wnd.on_action = wlistfilter_action;
	w->wnd.hide_on_close = 1;
	gg->wlistfilter = w;
}
