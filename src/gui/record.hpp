/** phiola: GUI: record audio
2023, Simon Zolin */

struct gui_wrecord {
	ffui_windowxx		wnd;
	ffui_labelxx		ldir, lname, lext, ldev, lchan, l_rate, luntil, laacq, lvorbisq, lopusq, lmp3q;
	ffui_editxx			edir, ename, e_rate, euntil, eaacq, evorbisq, eopusq, emp3q;
	ffui_comboboxxx		cbext, cbdev, cbchan;
	ffui_buttonxx		bstart;

	xxstr conf_dir, conf_name, conf_ext;
	uint conf_aacq, conf_vorbisq, conf_opusq, conf_mp3q;
	uint conf_until;
	uint conf_idev;
	uint conf_rate;
	uint conf_channels;
	char *wnd_pos;

	uint initialized;
};

#define _(m)  FFUI_LDR_CTL(gui_wrecord, m)
FF_EXTERN const ffui_ldr_ctl wrecord_ctls[] = {
	_(wnd),
	_(ldir),	_(edir),
	_(lname),	_(ename),
	_(lext),	_(cbext),
	_(ldev),	_(cbdev),
	_(lchan),	_(cbchan),
	_(l_rate),	_(e_rate),
	_(luntil),	_(euntil),
	_(laacq),	_(eaacq),
	_(lvorbisq),_(evorbisq),
	_(lopusq),	_(eopusq),
	_(lmp3q),	_(emp3q),
	_(bstart),
	FFUI_LDR_CTL_END
};
#undef _

#define O(m)  (void*)FF_OFF(gui_wrecord, m)
const ffarg wrecord_args[] = {
	{ "aacq",		'u',	O(conf_aacq) },
	{ "auto_stop",	'u',	O(conf_until) },
	{ "channels",	'u',	O(conf_channels) },
	{ "dir",		'=S',	O(conf_dir) },
	{ "ext",		'=S',	O(conf_ext) },
	{ "idev",		'u',	O(conf_idev) },
	{ "mp3q",		'u',	O(conf_mp3q) },
	{ "name",		'=S',	O(conf_name) },
	{ "opusq",		'u',	O(conf_opusq) },
	{ "rate",		'u',	O(conf_rate) },
	{ "vorbisq",	'u',	O(conf_vorbisq) },
	{ "wrecord.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

static uint wrec_vorbisq_conf(xxstr s)
{
	int n = s.int16(255);
	if (n == 255) {
		errlog("incorrect Vorbis quality '%S'", &s);
		return 0;
	}
	return (n + 1) * 10;
}

static int wrec_vorbisq_user(uint n) { return (int)n / 10 - 1; }

static int wrec_time_value(ffstr s)
{
	ffdatetime dt = {};
	if (s.len != fftime_fromstr1(&dt, s.ptr, s.len, FFTIME_HMS_MSEC_VAR)) {
		errlog("incorrect time value '%S'", &s);
		return 0;
	}

	fftime t;
	fftime_join1(&t, &dt);
	return fftime_to_msec(&t);
}

static char* wrec_time_str(char *buf, size_t cap, uint msec)
{
	fftime t;
	fftime_from_msec(&t, msec);
	ffdatetime dt = {};
	fftime_split1(&dt, &t);
	uint n = fftime_tostr1(&dt, buf, cap, FFTIME_HMS_MSEC);
	buf[n] = '\0';
	return buf;
}

static void wrecord_ui_to_conf()
{
	gui_wrecord *c = gg->wrecord;
	c->conf_dir.free();
	c->conf_name.free();
	c->conf_ext.free();
	c->conf_dir = c->edir.text();
	c->conf_name = c->ename.text();
	c->conf_ext = c->cbext.text();

	c->conf_idev = c->cbdev.get();
	c->conf_channels = c->cbchan.get();
	c->conf_rate = xxvec(c->e_rate.text()).str().uint32(0);
	c->conf_until = wrec_time_value(xxvec(c->euntil.text()).str());

	c->conf_aacq = xxvec(c->eaacq.text()).str().uint32(0);
	c->conf_vorbisq = wrec_vorbisq_conf(xxvec(c->evorbisq.text()).str());
	c->conf_opusq = xxvec(c->eopusq.text()).str().uint32(0);
	c->conf_mp3q = xxvec(c->emp3q.text()).str().uint32(~0U);
}

void wrecord_userconf_write(ffconfw *cw)
{
	gui_wrecord *w = gg->wrecord;
	if (w->initialized)
		wrecord_ui_to_conf();
	ffconfw_add2s(cw, "dir", w->conf_dir);
	ffconfw_add2s(cw, "name", w->conf_name);
	ffconfw_add2s(cw, "ext", w->conf_ext);
	ffconfw_add2u(cw, "idev", w->conf_idev);
	ffconfw_add2u(cw, "channels", w->conf_channels);
	ffconfw_add2u(cw, "rate", w->conf_rate);
	ffconfw_add2u(cw, "aacq", w->conf_aacq);
	ffconfw_add2u(cw, "vorbisq", w->conf_vorbisq);
	ffconfw_add2u(cw, "opusq", w->conf_opusq);
	ffconfw_add2u(cw, "mp3q", w->conf_mp3q);
	ffconfw_add2u(cw, "auto_stop", w->conf_until);

	if (w->initialized)
		conf_wnd_pos_write(cw, "wrecord.pos", &w->wnd);
	else if (w->wnd_pos)
		ffconfw_add2z(cw, "wrecord.pos", w->wnd_pos);
}

uint adevices_fill(uint flags, ffui_comboboxxx &cb, uint index)
{
	if (!adev_find_mod()) return 0;

	struct phi_adev_ent *ents;
	uint ndev = gd->adev_if->list(&ents, flags);

	cb.add("Default");
	for (uint i = 0;  i != ndev;  i++) {
		cb.add(ents[i].name);
	}

	if (index >= ndev + 1)
		index = 0;
	cb.set(index);

	gd->adev_if->list_free(ents);
	return index;
}

static void file_extensions_fill()
{
	gui_wrecord *w = gg->wrecord;
	uint index = 0 /*m4a*/;
	static const char oext[][5] = {
		"m4a",
		"ogg",
		"opus",
		"mp3",
		"flac",
		"wav",
	};
	for (uint i = 0;  i < FF_COUNT(oext);  i++) {
		w->cbext.add(oext[i]);
		if (w->conf_ext.equals(oext[i]))
			index = i;
	}
	w->cbext.set(index);
	w->conf_ext.free();
}

static void channels_fill()
{
	gui_wrecord *w = gg->wrecord;
	w->cbchan.add("Default");
	static const char chans[][11] = {
		"1 (Mono)",
		"2 (Stereo)",
		"3",
		"4",
		"5",
		"6 (5.1)",
		"7",
		"8",
	};
	for (uint i = 0;  i < FF_COUNT(chans);  i++) {
		w->cbchan.add(chans[i]);
	}
	w->cbchan.set(w->conf_channels);
}

static void wrecord_ui_from_conf()
{
	gui_wrecord *w = gg->wrecord;
	w->edir.text((w->conf_dir.len) ? w->conf_dir : gd->user_conf_dir);
	w->ename.text((w->conf_name.len) ? w->conf_name : "rec-@nowdate-@nowtime");
	w->conf_dir.free();
	w->conf_name.free();

	w->conf_idev = adevices_fill(PHI_ADEV_CAPTURE, w->cbdev, w->conf_idev);

	channels_fill();
	file_extensions_fill();

	xxstr_buf<100> s;
	if (w->conf_until)
		w->euntil.text(wrec_time_str(s.ptr, 100, w->conf_until));
	if (w->conf_rate)
		w->e_rate.text(s.zfmt("%u", w->conf_rate));
	w->eaacq.text(s.zfmt("%u", (w->conf_aacq) ? w->conf_aacq : 5));
	w->evorbisq.text(s.zfmt("%d", (w->conf_vorbisq) ? wrec_vorbisq_user(w->conf_vorbisq) : 7));
	w->eopusq.text(s.zfmt("%u", (w->conf_opusq) ? w->conf_opusq : 192));
	w->emp3q.text(s.zfmt("%u", (w->conf_mp3q != ~0U) ? w->conf_mp3q : 2));
}

static struct phi_track_conf* record_conf_create()
{
	gui_wrecord *w = gg->wrecord;
	struct phi_track_conf *c = ffmem_new(struct phi_track_conf);

	c->iaudio.device_index = w->conf_idev;
	c->iaudio.format.channels = w->conf_channels;
	c->iaudio.format.rate = w->conf_rate;
	// .iaudio.buf_time =
	c->until_msec = w->conf_until;
	// .afilter.gain_db =

	c->aac.quality = w->conf_aacq;
	c->opus.bitrate = w->conf_opusq;
	c->vorbis.quality = w->conf_vorbisq;
	c->mp3.quality = (w->conf_mp3q != ~0U) ? w->conf_mp3q + 1 : 0;

	c->ofile.name = ffsz_allocfmt("%S/%S.%S", &w->conf_dir, &w->conf_name, &w->conf_ext);
	return c;
}

void wrecord_start_stop()
{
	gui_wrecord *w = gg->wrecord;

	if (gd->recording_track) {
		gui_core_task((void(*)())record_stop);
		return;
	}

	if (w->initialized) {
		w->bstart.enable(0);
		wrecord_ui_to_conf();
	}
	wmain_status("Recording...");
	gui_core_task_ptr(record_begin, record_conf_create());
}

void wrecord_done()
{
	gui_wrecord *w = gg->wrecord;
	if (w->initialized)
		w->bstart.enable(1);
	wmain_status("Recording complete");
}

static void wrecord_action(ffui_window *wnd, int id)
{
	// gui_wrecord *w = gg->wrecord;
	switch (id) {
	case A_RECORD_START_STOP:
		wrecord_start_stop();  break;
	}
}

void wrecord_init()
{
	gui_wrecord *w = gui_allocT(gui_wrecord);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = wrecord_action;
	w->conf_mp3q = ~0U;
	gg->wrecord = w;
}

void wrecord_show(uint show)
{
	gui_wrecord *w = gg->wrecord;
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
		
		wrecord_ui_from_conf();

		if (gd->recording_track)
			w->bstart.enable(0);
	}

	w->wnd.show(1);
}
