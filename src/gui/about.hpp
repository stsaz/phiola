/** phiola: GUI: about
2023, Simon Zolin */

#define HOMEPAGE_URL  "https://github.com/stsaz/phiola"

struct gui_wabout {
	ffui_windowxx wnd;
	ffui_labelxx labout, lurl;
	ffui_image ico;
};

FF_EXTERN const ffui_ldr_ctl wabout_ctls[] = {
	FFUI_LDR_CTL(gui_wabout, wnd),
	FFUI_LDR_CTL(gui_wabout, ico),
	FFUI_LDR_CTL(gui_wabout, labout),
	FFUI_LDR_CTL(gui_wabout, lurl),
	FFUI_LDR_CTL_END
};

static void wabout_action(ffui_window *wnd, int id)
{
	switch (id) {
	case A_ABOUT_HOMEPAGE:
#ifdef FF_WIN
		ffui_shellexec(HOMEPAGE_URL, SW_SHOWNORMAL);
#endif
		break;
	}
}

void wabout_init()
{
	gui_wabout *a = ffmem_new(gui_wabout);
	a->wnd.hide_on_close = 1;
	a->wnd.on_action = wabout_action;
	gg->wabout = a;
}

void wabout_show(uint show)
{
	gui_wabout *a = gg->wabout;

	if (!show) {
		a->wnd.show(0);
		return;
	}

	char buf[255];
	ffsz_format(buf, sizeof(buf),
		"phiola v%s\n"
		"Fast audio player, recorder, converter\n"
		, core->version_str);
	a->labout.text(buf);

#ifdef FF_WIN
	a->lurl.text(HOMEPAGE_URL);
#else
	ffsz_format(buf, sizeof(buf), "<a href=\"%s\">%s</a>"
		, HOMEPAGE_URL, HOMEPAGE_URL);
	a->lurl.markup(buf);
#endif

	a->wnd.show(1);
}
