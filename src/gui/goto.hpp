/** phiola: GUI: go to audio position
2023, Simon Zolin */

struct gui_wgoto {
	ffui_windowxx	wnd;
	ffui_editxx		etime;
	ffui_buttonxx	bgo;
};

FF_EXTERN const ffui_ldr_ctl wgoto_ctls[] = {
	FFUI_LDR_CTL(gui_wgoto, wnd),
	FFUI_LDR_CTL(gui_wgoto, etime),
	FFUI_LDR_CTL(gui_wgoto, bgo),
	FFUI_LDR_CTL_END
};

static void play_goto(ffstrxx s)
{
	gui_wgoto *g = gg->wgoto;

	ffdatetime dt = {};
	if (s.len != fftime_fromstr1(&dt, s.ptr, s.len, FFTIME_HMS_MSEC_VAR))
		return;
	fftime t;
	fftime_join1(&t, &dt);
	g->wnd.show(0);

	gd->seek_pos_sec = fftime_sec(&t);
	gui_core_task_uint(ctl_action, A_SEEK);
}

static void wgoto_action(ffui_wnd *wnd, int id)
{
	gui_wgoto *g = gg->wgoto;

	switch (id) {
	case A_GOTO:
		play_goto(ffvecxx(g->etime.text()).str());  break;
	}
}

void wgoto_show(uint pos)
{
	gui_wgoto *g = gg->wgoto;
	g->etime.text(ffvecxx().addf("%02u:%02u", pos / 60, pos % 60).str());
	g->etime.sel_all();
	g->etime.focus();
	g->wnd.show(1);
}

void wgoto_init()
{
	gui_wgoto *g = ffmem_new(gui_wgoto);
	gg->wgoto = g;
	g->wnd.hide_on_close = 1;
	g->wnd.on_action = wgoto_action;
}
