/** phiola: GUI: convert files
2023, Simon Zolin */

struct gui_wconvert {
	ffui_wndxx wnd;
	ffui_labelxx lname, lext;
	ffui_editxx oname, oext;
	ffui_buttonxx bstart;

	ffvecxx conf_oname, conf_oext;
};

FF_EXTERN const ffui_ldr_ctl wconvert_ctls[] = {
	FFUI_LDR_CTL(gui_wconvert, wnd),
	FFUI_LDR_CTL(gui_wconvert, lname),
	FFUI_LDR_CTL(gui_wconvert, lext),
	FFUI_LDR_CTL(gui_wconvert, oname),
	FFUI_LDR_CTL(gui_wconvert, oext),
	FFUI_LDR_CTL(gui_wconvert, bstart),
	FFUI_LDR_CTL_END
};

void wconvert_userconf_write(ffvec *buf)
{
	gui_wconvert *c = gg->wconvert;
	ffvecxx name = c->oname.text(), ext = c->oext.text();
	ffvec_addfmt(buf, "oname.val \"%S\"\n", &name);
	ffvec_addfmt(buf, "oext.val \"%S\"\n", &ext);
}

int wconvert_userconf_read(ffstr key, ffstr val)
{
	gui_wconvert *c = gg->wconvert;
	ffstrxx k = key, v = val;

	if (k == "oname.val") {
		if (!v.matchf("\"%S\"", &val) && val.len)
			c->conf_oname.copy(val);

	} else if (k == "oext.val") {
		if (!v.matchf("\"%S\"", &val) && val.len)
			c->conf_oext.copy(val);

	} else {
		return -1;
	}

	return 0;
}

void wconvert_done()
{
	gui_wconvert *c = gg->wconvert;
	c->bstart.enable(1);
}

static struct phi_track_conf* conv_conf_create()
{
	gui_wconvert *c = gg->wconvert;
	struct phi_track_conf *tc = ffmem_new(struct phi_track_conf);
	ffvecxx name = c->oname.text(), ext = c->oext.text();
	tc->ofile.name = ffsz_allocfmt("%S.%S", &name, &ext);
	return tc;
}

static void wconvert_action(ffui_wnd *wnd, int id)
{
	gui_wconvert *c = gg->wconvert;
	switch (id) {
	case A_CONVERT_START:
		c->bstart.enable(0);
		gui_core_task_ptr(convert_begin, conv_conf_create());
		break;
	}
}

void wconvert_init()
{
	gui_wconvert *c = ffmem_new(gui_wconvert);
	c->wnd.hide_on_close = 1;
	c->wnd.on_action = wconvert_action;
	c->conf_oname.set("@filepath/@filename");
	c->conf_oext.set("m4a");
	gg->wconvert = c;
}

void wconvert_show(uint show, ffslice items)
{
	gui_wconvert *c = gg->wconvert;

	if (!show) {
		c->wnd.show(0);
		return;
	}

	if (c->conf_oname.len) {
		c->oname.text(c->conf_oname.str());
		c->oext.text(c->conf_oext.str());
		c->conf_oname.free();
		c->conf_oext.free();
	}
	gui_core_task_slice(convert_add, items);
	c->wnd.show(1);
}
