/** phiola: gui-winapi: log
2021, Simon Zolin */

struct gui_wlog {
	ffui_windowxx	wnd;
	ffui_textxx		tlog;

	fflock	lock;
	xxvec	buf;

	char *wnd_pos;
	uint initialized :1;
};

FF_EXTERN const ffui_ldr_ctl wlog_ctls[] = {
	FFUI_LDR_CTL(gui_wlog, wnd),
	FFUI_LDR_CTL(gui_wlog, tlog),
	FFUI_LDR_CTL_END
};

#define O(m)  (void*)FF_OFF(gui_wlog, m)
const ffarg wlog_args[] = {
	{ "wlog.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

void wlog_userconf_write(ffconfw *cw)
{
	gui_wlog *w = gg->wlog;
	if (w->initialized)
		conf_wnd_pos_write(cw, "wlog.pos", &w->wnd);
	else if (w->wnd_pos)
		ffconfw_add2z(cw, "wlog.pos", w->wnd_pos);
}

static void wlog_add()
{
	gui_wlog *w = gg->wlog;

	fflock_lock(&w->lock);
	xxstr d = w->buf.str();
	w->buf.reset();
	fflock_unlock(&w->lock);

	w->tlog.add(d);
	d.free();

	if (!w->initialized) {
		w->initialized = 1;

		if (w->wnd_pos)
			conf_wnd_pos_read(&w->wnd, FFSTR_Z(w->wnd_pos));
		ffmem_free(w->wnd_pos);
	}

	w->wnd.show(1);
}

/**
Thread: worker */
extern "C" void gui_log(void *udata, ffstr s)
{
	FF_ASSERT(gg && gg->wlog && gg->wlog->wnd.h);

	gui_wlog *w = gg->wlog;

	fflock_lock(&w->lock);
	w->buf.add(s);
	fflock_unlock(&w->lock);
	gui_task(wlog_add);
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
