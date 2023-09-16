/** phiola: GUI: convert files
2023, Simon Zolin */

struct gui_wconvert {
	ffui_wndxx			wnd;
	ffui_labelxx		ldir, lname, lext;
	ffui_editxx			edir, ename, eext;
	ffui_buttonxx		bstart;
	ffui_labelxx		laacq;
	ffui_editxx			eaacq;
	ffui_checkboxxx		cbcopy;

	ffstrxx conf_edir, conf_ename, conf_eext;
	uint conf_cbcopy;
	uint conf_eaacq;
	uint initialized :1;
};

FF_EXTERN const ffui_ldr_ctl wconvert_ctls[] = {
	FFUI_LDR_CTL(gui_wconvert, wnd),
	FFUI_LDR_CTL(gui_wconvert, ldir),
	FFUI_LDR_CTL(gui_wconvert, lname),
	FFUI_LDR_CTL(gui_wconvert, lext),
	FFUI_LDR_CTL(gui_wconvert, edir),
	FFUI_LDR_CTL(gui_wconvert, ename),
	FFUI_LDR_CTL(gui_wconvert, eext),
	FFUI_LDR_CTL(gui_wconvert, laacq),
	FFUI_LDR_CTL(gui_wconvert, eaacq),
	FFUI_LDR_CTL(gui_wconvert, cbcopy),
	FFUI_LDR_CTL(gui_wconvert, bstart),
	FFUI_LDR_CTL_END
};

#define O(m)  (void*)FF_OFF(gui_wconvert, m)
const ffarg wconvert_args[] = {
	{ "cbcopy",	'u',	O(conf_cbcopy) },
	{ "eaacq",	'u',	O(conf_eaacq) },
	{ "edir",	'=S',	O(conf_edir) },
	{ "eext",	'=S',	O(conf_eext) },
	{ "ename",	'=S',	O(conf_ename) },
	{}
};
#undef O

void wconvert_userconf_write(ffvec *buf)
{
	gui_wconvert *c = gg->wconvert;
	ffvecxx dir = c->edir.text()
		, name = c->ename.text()
		, ext = c->eext.text()
		, aacq = c->eaacq.text();
	ffvec_addfmt(buf, "\tedir \"%S\"\n", &dir);
	ffvec_addfmt(buf, "\tename \"%S\"\n", &name);
	ffvec_addfmt(buf, "\teext \"%S\"\n", &ext);
	ffvec_addfmt(buf, "\tcbcopy %u\n", c->cbcopy.checked());
	ffvec_addfmt(buf, "\teaacq %u\n", aacq.str().uint16(0));
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
	ffvecxx dir = c->edir.text()
		, name = c->ename.text()
		, ext = c->eext.text()
		, aacq = c->eaacq.text();
	tc->ofile.name = ffsz_allocfmt("%S/%S.%S", &dir, &name, &ext);
	tc->aac.quality = aacq.str().uint16(0);
	tc->stream_copy = c->cbcopy.checked();
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
	gg->wconvert = c;
}

void wconvert_show(uint show, ffslice items)
{
	gui_wconvert *c = gg->wconvert;

	if (!show) {
		c->wnd.show(0);
		return;
	}

	if (!c->initialized) {
		c->initialized = 1;

		c->edir.text((c->conf_edir.len) ? c->conf_edir : "@filepath");
		c->ename.text((c->conf_ename.len) ? c->conf_ename : "@filename");
		c->eext.text((c->conf_eext.len) ? c->conf_eext : "m4a");
		c->conf_edir.free();
		c->conf_ename.free();
		c->conf_eext.free();

		c->cbcopy.check(!!c->conf_cbcopy);

		char buf[100];
		ffsz_format(buf, sizeof(buf), "%u", (c->conf_eaacq) ? c->conf_eaacq : 5);
		c->eaacq.text(buf);
	}

	gui_core_task_slice(convert_add, items);
	c->wnd.show(1);
}
