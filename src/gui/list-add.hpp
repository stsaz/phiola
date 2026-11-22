/** phiola: GUI: add URL
2023, Simon Zolin */

struct gui_wlistadd {
	ffui_windowxx	wnd;
	ffui_label		lname;
	ffui_editxx		turi;
	ffui_buttonxx	badd;
	uint initialized :1;
};

FF_EXTERN const ffui_ldr_ctl wlistadd_ctls[] = {
	FFUI_LDR_CTL(gui_wlistadd, wnd),
	FFUI_LDR_CTL(gui_wlistadd, lname),
	FFUI_LDR_CTL(gui_wlistadd, turi),
	FFUI_LDR_CTL(gui_wlistadd, badd),
	FFUI_LDR_CTL_END
};

static void wlistadd_action(ffui_window *wnd, int id)
{
	gui_wlistadd *w = gg->wlistadd;

	switch (id) {
	case A_LISTADD_ADD: {
		ffstr s = w->turi.text();
		if (s.len)
			gui_core_task_ptr(list_add_sz, s.ptr);
		else
			ffstr_free(&s);
		w->wnd.show(0);
		break;
	}
	}
}

void wlistadd_show()
{
	gui_wlistadd *w = gg->wlistadd;
	w->initialized = 1;
	w->turi.focus();
	w->wnd.show(1);
	w->wnd.present();
}

static void wlistadd_init()
{
	gui_wlistadd *w = gui_allocT(gui_wlistadd);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = wlistadd_action;
	gg->wlistadd = w;
}

void wlistadd_cmd(uint cmd)
{
	gui_wlistadd *w = gg->wlistadd;
	switch (cmd) {
	case GUI_CMD_INIT:
		wlistadd_init();  break;
	case GUI_CMD_HIDE:
		if (w->initialized)
			w->wnd.show(0);
		break;
	}
}
