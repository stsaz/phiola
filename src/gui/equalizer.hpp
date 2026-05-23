/** phiola: GUI: Settings: Equalizer
2026, Simon Zolin */

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

struct gui_weqlz {
	ffui_windowxx	wnd;
	ffui_comboboxxx	cbband;
	ffui_labelxx	lfreq, lwidth, lgain;
	ffui_trackbarxx	tbfreq, tbwidth, tbgain;
	ffui_editxx		eeqlz;

	struct eqlz_set		eqlz;
	struct eqlz_band*	band;
	uint	initialized :1;
};

#define _(m)  FFUI_LDR_CTL(gui_weqlz, m)
FF_EXTERN const ffui_ldr_ctl weqlz_ctls[] = {
	_(cbband),
	_(lfreq),	_(tbfreq),
	_(lwidth),	_(tbwidth),
	_(lgain),	_(tbgain),
	_(eeqlz),
};
#undef _

static void weqlz_action(ffui_window *wnd, int id)
{
	gui_weqlz *w = gg->weqlz;
	switch (id) {
	case A_EQ_CHBAND:
		w->band = &w->eqlz.select_band(w->cbband.get());
		w->tbfreq.enable(!w->band->shelf());
		w->tbwidth.enable(!w->band->shelf());
		w->tbfreq.set(w->band->freq_progress());
		w->tbwidth.set(w->band->width_progress());
		w->tbgain.set(w->band->gain_progress());
		break;

	case A_EQ_FREQ:
		w->band->freq_set(w->tbfreq.get());
		goto eq_apply;

	case A_EQ_WIDTH:
		w->band->width_set(w->tbwidth.get());
		goto eq_apply;

	case A_EQ_GAIN:
		w->band->gain_set(w->tbgain.get());

	eq_apply:
		w->eeqlz.text(w->eqlz.str().strz());
		break;

	case A_EQ_CLOSED:
		ffmem_free(gg->eqlz);
		gg->eqlz = w->eeqlz.text().ptr;
		break;
	}
}

void weqlz_show(uint show)
{
	gui_weqlz *w = gg->weqlz;
	if (gui_dlg_load())
		return;

	if (!show) {
		w->wnd.show(0);
		return;
	}

	if (!w->initialized) {
		w->initialized = 1;

		w->eqlz.init(gd->conf.eqlz);
		static const char bands[][12] = {
			"Low Shelf",
			"Band #2",
			"Band #3",
			"Band #4",
			"High Shelf",
		};
		for (uint i = 0;  i < FF_COUNT(bands);  i++) {
			w->cbband.add(bands[i]);
		}
		w->cbband.set(0);
		weqlz_action(&w->wnd, A_EQ_CHBAND);

		w->eeqlz.text(gd->conf.eqlz);
	}

	w->wnd.show(1);
	w->wnd.present();
}

void weqlz_init()
{
	gui_weqlz *w = gui_allocT(gui_weqlz);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = weqlz_action;
	w->wnd.onclose_id = A_EQ_CLOSED;
	gg->weqlz = w;
}
