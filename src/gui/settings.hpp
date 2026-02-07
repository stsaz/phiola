/** phiola: GUI: Settings
2023, Simon Zolin */

struct eqlz_band {
	uint type;
	uint freq;
	uint width;
	int gain;

	bool shelf() const { return !!(type & 1); }

	void parse(xxstr s) {
		xxstr k, v;
		while (s.len) {
			s.split_by(' ', &k, &s);
			s.split_by(' ', &v, &s);

			if (k.equals("f")) {
				freq = v.uint32(0);

			} else if (k.equals("w")) {
				if (v.len && v.last_char() == 'q') {
					v.len--; // cut last 'q'
					width = v.float64(0) * 10;
				}

			} else if (k.equals("g")) {
				gain = v.float64(0) * 10;
			}
		}
	}

	void freq_set(uint progress) {
		freq = (20000 - 20) * pow((double)(progress + 1) / 100, 3) + 20;
	}
	uint freq_progress() const { return 0; }

	void width_set(uint progress) { width = progress; }
	uint width_progress() const { return width; }

	void gain_set(uint progress) { gain = (progress * 5) - 120; }
	uint gain_progress() const { return (gain + 120) / 5; }

	void str(xxvec *v) const {
		if (gain == 0)
			return;

		if (type & 1) {
			v->add_f("t %s g %.01F"
				, (type == 1) ? "bass" : "treble", (double)gain / 10);
		} else {
			v->add_f("f %u w %.01Fq g %.01F"
				, freq, (double)width / 10, (double)gain / 10);
		}
	}
};

struct eqlz_set {
	uint current;
	struct eqlz_band bands[5];

	void init(const char *sz) {
		bands[0].type = 1;
		bands[1].width = bands[2].width = bands[3].width = 10; // =1.0
		bands[4].type = 3;

		if (!sz)
			return;

		uint i = 0;
		xxstr s(sz), pt;
		while (s.len && i < FF_COUNT(bands)) {
			s.split_by(',', &pt, &s);
			bands[i++].parse(pt);
		}
	}

	struct eqlz_band& select_band(uint i) {
		current = i;
		return bands[current];
	}

	xxvec str() const {
		xxvec v;
		for (uint i = 0;  i < FF_COUNT(bands);  i++) {
			bands[i].str(&v);
			v.add(",");
		}
		v.len--; // cut last ','
		return v;
	}
};

struct gui_wsettings {
	ffui_windowxx	wnd;
	ffui_label		ldev, lseek_by, lleap_by, lauto_skip;
	ffui_editxx		eseek_by, eleap_by, eauto_skip;
	ffui_checkboxxx	cbdarktheme, cbrg_norm, cbauto_norm;
	ffui_comboboxxx	cbdev;

	ffui_checkboxxx		cbeqlz;
	ffui_comboboxxx		cbeqlz_band;
	ffui_trackbarxx		tbeqlz_freq, tbeqlz_width, tbeqlz_gain;
	ffui_editxx			eeqlz;
	struct eqlz_set		eqlz;
	struct eqlz_band*	eqlz_band;

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
	_(cbrg_norm),
	_(cbauto_norm),
	_(cbeqlz), _(cbeqlz_band), _(tbeqlz_freq), _(tbeqlz_width), _(tbeqlz_gain), _(eeqlz),
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

	gd->conf.auto_norm = w->cbauto_norm.checked();
	gd->conf.rg_norm = w->cbrg_norm.checked() && !gd->conf.auto_norm;
	gd->conf.odev = w->cbdev.get();

	struct gui_conf *conf = ffmem_new(struct gui_conf);
	conf->eqlz_on = w->cbeqlz.checked();
	conf->eqlz = w->eeqlz.text().ptr;
	gui_core_task_ptr(list_conf_set, conf);

	gd->conf.seek_step_delta = xxvec(w->eseek_by.text()).str().uint32(10);
	gd->conf.seek_leap_delta = xxvec(w->eleap_by.text()).str().uint32(60);
	gd->conf.auto_skip_sec_percent = auto_skip_read(xxvec(w->eauto_skip.text()).str());
}

static void wsettings_ui_from_conf()
{
	gui_wsettings *w = gg->wsettings;

	if (w->cbdarktheme.h)
		w->cbdarktheme.check(!!gd->conf.theme);

	w->cbrg_norm.check(!!gd->conf.rg_norm);
	w->cbauto_norm.check(!!gd->conf.auto_norm);

	uint odev = adevices_fill(PHI_ADEV_PLAYBACK, w->cbdev, gd->conf.odev);
	if (gd->conf.odev != odev) {
		gd->conf.odev = odev;
	}

	xxstr_buf<100> s;
	w->eseek_by.text(s.zfmt("%u", gd->conf.seek_step_delta));
	w->eleap_by.text(s.zfmt("%u", gd->conf.seek_leap_delta));
	w->eauto_skip.text(auto_skip_write(s));

	w->cbeqlz.check(gd->conf.eqlz_on);
	w->eeqlz.text(gd->conf.eqlz);
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
	gui_wsettings *w = gg->wsettings;
	switch (id) {
	case A_EQ_CHBAND:
		w->eqlz_band = &w->eqlz.select_band(w->cbeqlz_band.get());
		w->tbeqlz_freq.enable(!w->eqlz_band->shelf());
		w->tbeqlz_width.enable(!w->eqlz_band->shelf());
		w->tbeqlz_freq.set(w->eqlz_band->freq_progress());
		w->tbeqlz_width.set(w->eqlz_band->width_progress());
		w->tbeqlz_gain.set(w->eqlz_band->gain_progress());
		break;

	case A_EQ_FREQ:
		w->eqlz_band->freq_set(w->tbeqlz_freq.get());
		goto eq_apply;

	case A_EQ_WIDTH:
		w->eqlz_band->width_set(w->tbeqlz_width.get());
		goto eq_apply;

	case A_EQ_GAIN:
		w->eqlz_band->gain_set(w->tbeqlz_gain.get());

	eq_apply:
		w->eeqlz.text(w->eqlz.str().strz());
		break;

	case A_SETTINGS_APPLY:
		wsettings_ui_to_conf();  break;
	}
}

void wsettings_show(uint show)
{
	gui_wsettings *w = gg->wsettings;
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

		w->eqlz.init(gd->conf.eqlz);
		static const char eqlz_bands[][12] = {
			"Low Shelf",
			"Band #2",
			"Band #3",
			"Band #4",
			"High Shelf",
		};
		for (uint i = 0;  i < FF_COUNT(eqlz_bands);  i++) {
			w->cbeqlz_band.add(eqlz_bands[i]);
		}
		w->cbeqlz_band.set(0);
		wsettings_action(&w->wnd, A_EQ_CHBAND);

		wsettings_ui_from_conf();
	}

	w->wnd.show(1);
}

void wsettings_init()
{
	gui_wsettings *w = gui_allocT(gui_wsettings);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = wsettings_action;
	w->wnd.onclose_id = A_SETTINGS_APPLY;
	gg->wsettings = w;
}
