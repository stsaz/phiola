/** phiola: GUI: Settings
2023, Simon Zolin */

struct gui_wsettings {
	ffui_windowxx	wnd;
	ffui_label		lseek_by, lleap_by;
	ffui_editxx		eseek_by, eleap_by;
	ffui_checkboxxx	cbdarktheme;

	char*	wnd_pos;
	uint	initialized :1;
};

#define _(m)  FFUI_LDR_CTL(gui_wsettings, m)
FF_EXTERN const ffui_ldr_ctl wsettings_ctls[] = {
	_(wnd),
	_(cbdarktheme),
	_(lseek_by),	_(eseek_by),
	_(lleap_by),	_(eleap_by),
	FFUI_LDR_CTL_END
};
#undef _

#define O(m)  (void*)FF_OFF(gui_wsettings, m)
const ffarg wsettings_args[] = {
	{ "wsettings.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

static void wsettings_ui_to_conf()
{
	gui_wsettings *w = gg->wsettings;

	if (w->cbdarktheme.h)
		theme_switch(w->cbdarktheme.checked());

	gd->conf.seek_step_delta = ffvecxx(w->eseek_by.text()).str().uint32(10);
	gd->conf.seek_leap_delta = ffvecxx(w->eleap_by.text()).str().uint32(60);
}

static void wsettings_ui_from_conf()
{
	gui_wsettings *w = gg->wsettings;

	if (w->cbdarktheme.h)
		w->cbdarktheme.check(!!gd->conf.theme);

	ffstrxx_buf<100> s;
	w->eseek_by.text(s.zfmt("%u", gd->conf.seek_step_delta));
	w->eleap_by.text(s.zfmt("%u", gd->conf.seek_leap_delta));
}

void wsettings_userconf_write(ffconfw *cw)
{
	gui_wsettings *w = gg->wsettings;
	if (w->initialized)
		conf_wnd_pos_write(cw, "wsettings.pos", &w->wnd);
	else if (w->wnd_pos)
		ffconfw_add2z(cw, "wsettings.pos", w->wnd_pos);
}

static void wsettings_action(ffui_window *wnd, int id)
{
	// gui_wsettings *w = gg->wsettings;
	switch (id) {
	case A_SETTINGS_APPLY:
		wsettings_ui_to_conf();  break;
	}
}

void wsettings_show(uint show)
{
	gui_wsettings *w = gg->wsettings;

	if (!show) {
		w->wnd.show(0);
		return;
	}

	if (!w->initialized) {
		w->initialized = 1;

		if (w->wnd_pos)
			conf_wnd_pos_read(&w->wnd, FFSTR_Z(w->wnd_pos));
		ffmem_free(w->wnd_pos);

		wsettings_ui_from_conf();
	}

	w->wnd.show(1);
}

void wsettings_init()
{
	gui_wsettings *w = ffmem_new(gui_wsettings);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = wsettings_action;
	w->wnd.onclose_id = A_SETTINGS_APPLY;
	gg->wsettings = w;
}
