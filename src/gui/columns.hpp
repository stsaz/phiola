/** phiola: GUI: Configure Columns
2026, Simon Zolin */

struct gui_columns {
	ffui_windowxx	wnd;
	ffui_viewxx		vlist;

	char *wnd_pos;
	uint initialized :1;

	void fill() {
		this->vlist.clear();
		static const char col_vars[][12] = {
			"#",              // H_INDEX
			"$LCArtist",      // H_ARTIST
			"$LCTitle",       // H_TITLE
			"$LCDuration",    // H_DUR
			"$LCInfo",        // H_INFO
			"$LCDate",        // H_DATE
			"$LCAlbum",       // H_ALBUM
			"$LCFilename",    // H_FN
		};
		for (uint i = 0;  i < FF_COUNT(col_vars);  i++) {
			xxstr s = vars_val(&gg->ldr.vars, FFSTR_Z(col_vars[i]));
			this->vlist.append(s);
		}

		uint mask = wmain_cols_visible();
		while (mask) {
			uint i = ffbit_rfind32(mask) - 1;
			ffbit_reset32(&mask, i);
			this->vlist.check(i, 1);
		}
	}

	void apply() {
		xxvec v(this->vlist.checked());
		uint mask = 0;
		uint *it;
		FFSLICE_WALK(&v, it) {
			mask |= (1U << *it);
		}
		wmain_cols_visible_set(mask);
	}
};

FF_EXTERN const ffui_ldr_ctl wcolumns_ctls[] = {
	FFUI_LDR_CTL(gui_columns, wnd),
	FFUI_LDR_CTL(gui_columns, vlist),
	FFUI_LDR_CTL_END
};

#define O(m)  (void*)FF_OFF(gui_columns, m)
const ffarg wcolumns_args[] = {
	{ "pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

void wcolumns_userconf_write(ffconfw *cw)
{
	gui_columns *w = gg->wcolumns;
	if (w->initialized) {
		w->wnd_pos = wnd_pos_sz(&w->wnd);
	}
	ffarg_write_conf(cw, wcolumns_args, w);
	ffmem_free(w->wnd_pos);
}

static void wcolumns_action(ffui_window *wnd, int id)
{
	gui_columns *w = gg->wcolumns;
	switch (id) {
	case A_COLUMNS_CLOSED:
		w->apply();  break;
	}
}

void wcolumns_show(uint show)
{
	gui_columns *w = gg->wcolumns;
	if (gui_dlg_load())
		return;

	if (!show) {
		w->wnd.show(0);
		return;
	}

	if (!w->initialized) {
		w->initialized = 1;

		if (w->wnd_pos)
			conf_wnd_pos_read(&w->wnd, FFSTR_Z(w->wnd_pos));
		ffmem_free(w->wnd_pos);
		w->wnd_pos = NULL;
	}

	w->fill();
	w->wnd.show(1);
	w->wnd.present();
}

void wcolumns_init()
{
	gui_columns *w = gui_allocT(gui_columns);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = wcolumns_action;
	w->wnd.onclose_id = A_COLUMNS_CLOSED;
	gg->wcolumns = w;
}

void wcolumns_fin()
{
	gui_columns *w = gg->wcolumns;
	w->~gui_columns();
}