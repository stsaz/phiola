/** phiola: GUI: Settings
2023, Simon Zolin */

struct gui_wsettings {
	ffui_windowxx	wnd;
	ffui_label		ldev, lseek_by, lleap_by, lauto_skip;
	ffui_editxx		eseek_by, eleap_by, eauto_skip;
	ffui_checkboxxx	cbdarktheme;
	ffui_comboboxxx	cbdev;

	char*	wnd_pos;
	uint	conf_odev;
	uint	initialized :1;
};

#define _(m)  FFUI_LDR_CTL(gui_wsettings, m)
FF_EXTERN const ffui_ldr_ctl wsettings_ctls[] = {
	_(wnd),
	_(cbdarktheme),
	_(ldev),		_(cbdev),
	_(lseek_by),	_(eseek_by),
	_(lleap_by),	_(eleap_by),
	_(lauto_skip),	_(eauto_skip),
	FFUI_LDR_CTL_END
};
#undef _

#define O(m)  (void*)FF_OFF(gui_wsettings, m)
const ffarg wsettings_args[] = {
	{ "wsettings.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

static const char* auto_skip_write(xxstr_buf<100> &s)
{
	s.zfmt("%u", ffint_abs(gd->conf.auto_skip_sec_percent));
	if (gd->conf.auto_skip_sec_percent < 0) {
		s.add_char('%').add_char('\0');
	}
	return s.ptr;
}

static int auto_skip_read(xxstr s)
{
	if (s.len && s.at(s.len - 1) == '%') {
		s.len--;
		return -(int)s.uint32(0);
	}
	return s.uint32(0);
}

static void wsettings_ui_to_conf()
{
	gui_wsettings *w = gg->wsettings;

	if (w->cbdarktheme.h)
		theme_switch(w->cbdarktheme.checked());

	uint mod = 0, r;

	if (gd->conf.odev != (r = w->cbdev.get())) {
		gd->conf.odev = r;
		mod = 1;
	}

	if (mod) {
		// Apply settings for the active playlist
		struct phi_queue_conf *qc = gd->queue->conf(NULL);
		qc->tconf.oaudio.device_index = gd->conf.odev;
	}

	gd->conf.seek_step_delta = xxvec(w->eseek_by.text()).str().uint32(10);
	gd->conf.seek_leap_delta = xxvec(w->eleap_by.text()).str().uint32(60);
	gd->conf.auto_skip_sec_percent = auto_skip_read(xxvec(w->eauto_skip.text()).str());
}

static void wsettings_ui_from_conf()
{
	gui_wsettings *w = gg->wsettings;

	if (w->cbdarktheme.h)
		w->cbdarktheme.check(!!gd->conf.theme);

	uint odev = adevices_fill(PHI_ADEV_PLAYBACK, w->cbdev, gd->conf.odev);
	if (gd->conf.odev != odev) {
		gd->conf.odev = odev;
	}

	xxstr_buf<100> s;
	w->eseek_by.text(s.zfmt("%u", gd->conf.seek_step_delta));
	w->eleap_by.text(s.zfmt("%u", gd->conf.seek_leap_delta));
	w->eauto_skip.text(auto_skip_write(s));
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
