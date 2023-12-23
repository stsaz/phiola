/** phiola: gui-winapi: log
2021, Simon Zolin */

struct gui_wlog {
	ffui_windowxx	wnd;
	ffui_textxx		tlog;
};

FF_EXTERN const ffui_ldr_ctl wlog_ctls[] = {
	FFUI_LDR_CTL(gui_wlog, wnd),
	FFUI_LDR_CTL(gui_wlog, tlog),
	FFUI_LDR_CTL_END
};

extern "C" void phi_gui_log(ffstr s)
{
	if (!gg || !gg->wlog) return;

	gui_wlog *w = gg->wlog;
	w->tlog.add(s);
	w->wnd.show(1);
}

static void wlog_action(ffui_window *wnd, int id)
{
}

void wlog_init()
{
	gui_wlog *w = ffmem_new(gui_wlog);
	w->wnd.on_action = wlog_action;
	w->wnd.hide_on_close = 1;
	gg->wlog = w;
}
