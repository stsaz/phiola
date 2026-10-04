/** phiola: GUI: record audio
2023, Simon Zolin */

#include <gui/ac.hpp>

struct gui_wrecord {
	ffui_windowxx		wnd;
	ffui_labelxx		ldir, lname, lext, ldev, lchan, l_rate, luntil;
	ffui_editxx			edir, ename, e_rate, euntil;
	ffui_comboboxxx		cbext, cbdev, cbchan;
	ffui_checkboxxx		cbloopback, cbexcl;
	ffui_buttonxx		bbrowse, bstart;

	struct gui_ac ac;

	xxstr conf_dir, conf_name, conf_ext;
	uint conf_until;
	uint conf_idev;
	uint conf_rate;
	uint conf_channels;
	u_char conf_exclusive;
	u_char conf_loopback;
	char *wnd_pos;

	uint initialized;
};

#define _ac(ctl) \
	{ #ctl, (uint)FF_OFF(gui_wrecord, ac.ctl), NULL }

#define _(m)  FFUI_LDR_CTL(gui_wrecord, m)
FF_EXTERN const ffui_ldr_ctl wrecord_ctls[] = {
	_(wnd),
	_(bbrowse),
	_(ldir),	_(edir),
	_(lname),	_(ename),
	_(lext),	_(cbext),
	_(ldev),	_(cbdev),
	_(cbloopback),
	_(cbexcl),
	_(lchan),	_(cbchan),
	_(l_rate),	_(e_rate),
	_(luntil),	_(euntil),
	_ac(laacq),		_ac(eaacq),		_ac(tbaacq),
	_ac(lvorbisq),	_ac(evorbisq),	_ac(tbvorbisq),
	_ac(lopusq),	_ac(eopusq),	_ac(tbopusq),
	_ac(lmp3q),		_ac(emp3q),		_ac(tbmp3q),
	_(bstart),
	FFUI_LDR_CTL_END
};
#undef _
#undef _ac

#define O(m)  (void*)FF_OFF(gui_wrecord, m)
const ffarg wrecord_args[] = {
	{ "aacq",		'u',	O(ac.conf_aacq) },
	{ "auto_stop",	'u',	O(conf_until) },
	{ "channels",	'u',	O(conf_channels) },
	{ "dir",		'=S',	O(conf_dir) },
	{ "exclusive",	'b',	O(conf_exclusive) },
	{ "ext",		'=S',	O(conf_ext) },
	{ "idev",		'u',	O(conf_idev) },
	{ "loopback",	'b',	O(conf_loopback) },
	{ "mp3q",		'u',	O(ac.conf_mp3q) },
	{ "name",		'=S',	O(conf_name) },
	{ "opusq",		'u',	O(ac.conf_opusq) },
	{ "rate",		'u',	O(conf_rate) },
	{ "vorbisq",	'u',	O(ac.conf_vorbisq) },
	{ "wrecord.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

static void check_safe(ffui_checkboxxx &cb, bool val) { if (cb.h) cb.check(val); }
static bool checked_safe(ffui_checkboxxx &cb) { return (cb.h) ? cb.checked() : 0; }

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

static void wrec_browse()
{
	gui_wrecord *w = gg->wrecord;

	xxvec v;
	v.add_f("%S%c%S.%S"
		, &xxvec(w->edir.text()).str()
		, FFPATH_SLASH
		, &xxvec(w->ename.text()).str()
		, &xxvec(w->cbext.text()).str());

	char *fn;
	if (!(fn = gui_dlg_save(&w->wnd, (char*)v.ptr, v.len)))
		return;

	ffstr path, name, ext = {};
	ffpath_split3_str(FFSTR_Z(fn), &path, &name, &ext);

	w->edir.text(path);
	w->ename.text(name);

	int i = out_file_ext_index(ext);
	if (i >= 0)
		w->cbext.set(i);
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
	c->conf_loopback = checked_safe(c->cbloopback);
	c->conf_exclusive = checked_safe(c->cbexcl);
	c->conf_channels = c->cbchan.get();
	c->conf_rate = xxvec(c->e_rate.text()).str().uint32(0);
	c->conf_until = wrec_time_value(xxvec(c->euntil.text()).str());

	c->ac.ui_to_conf();
}

void wrecord_userconf_write(ffconfw *cw)
{
	gui_wrecord *w = gg->wrecord;
	if (w->initialized) {
		wrecord_ui_to_conf();
		w->wnd_pos = wnd_pos_sz(&w->wnd);
	}
	ffarg_write_conf(cw, wrecord_args, w);
	ffmem_free(w->wnd_pos);
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

static void wrec_ext_chg(uint i)
{
	gui_wrecord *w = gg->wrecord;
	w->ac.enable_controls(out_fmt[i].fmt);
}

static void file_extensions_fill()
{
	gui_wrecord *w = gg->wrecord;
	uint index = 0 /*m4a*/;
	for (uint i = 0;  i < FF_COUNT(out_fmt);  i++) {
		w->cbext.add(out_fmt[i].ext);
		if (w->conf_ext.equals(out_fmt[i].ext))
			index = i;
	}
	w->cbext.set(index);
	wrec_ext_chg(index);
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
	xxstr ss = (w->conf_dir.len) ? w->conf_dir : gd->user_conf_dir;
	if (ss.len && ffpath_slash(ss.last_char()))
		ss.len--;
	w->edir.text(ss);
	w->ename.text((w->conf_name.len) ? w->conf_name : "rec-@nowdate-@nowtime");
	w->conf_dir.free();
	w->conf_name.free();

	check_safe(w->cbloopback, w->conf_loopback);
	w->conf_idev = adevices_fill((w->conf_loopback) ? PHI_ADEV_PLAYBACK : PHI_ADEV_CAPTURE, w->cbdev, w->conf_idev);
	check_safe(w->cbexcl, w->conf_exclusive);

	channels_fill();
	file_extensions_fill();

	xxstr_buf<100> s;

	if (w->conf_until)
		w->euntil.text(wrec_time_str(s.ptr, 100, w->conf_until));
	if (!w->conf_rate)
		w->conf_rate = 44100;
	w->e_rate.text(s.zfmt("%u", w->conf_rate));

	w->ac.init_from_conf();
}

static struct phi_track_conf* record_conf_create()
{
	gui_wrecord *w = gg->wrecord;
	struct phi_track_conf *c = ffmem_new(struct phi_track_conf);

	c->iaudio.device_index = w->conf_idev;
	c->iaudio.exclusive = w->conf_exclusive;
	c->iaudio.loopback = w->conf_loopback;
	c->iaudio.format.channels = w->conf_channels;
	c->iaudio.format.rate = w->conf_rate;
	// .iaudio.buf_time =
	c->until_msec = w->conf_until;
	// .afilter.gain_db =

	c->aac.quality = w->ac.conf_aacq;
	c->opus.bitrate = w->ac.conf_opusq;
	c->vorbis.quality = (w->ac.conf_vorbisq != ~0U) ? (w->ac.conf_vorbisq + 1) * 10 : 0;
	c->mp3.quality = (w->ac.conf_mp3q != ~0U) ? w->ac.conf_mp3q + 1 : 0;

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
	gui_wrecord *w = gg->wrecord;
	switch (id) {
	case A_REC_LOOPBACK:
		w->conf_loopback = checked_safe(w->cbloopback);
		if (w->cbexcl.h)
			w->cbexcl.enable(!w->conf_loopback);
		w->cbdev.clear();
		w->conf_idev = adevices_fill((w->conf_loopback) ? PHI_ADEV_PLAYBACK : PHI_ADEV_CAPTURE, w->cbdev, w->conf_idev);
		break;

	case A_REC_BROWSE:
		wrec_browse();  break;

	case A_REC_EXT_CHG:
		wrec_ext_chg(w->cbext.get());  break;

	case A_CO_AACQ:
	case A_CO_VORBISQ:
	case A_CO_OPUSQ:
	case A_CO_MP3Q:
		w->ac.on_trackbar(id);  break;

	case A_RECORD_START_STOP:
		wrecord_start_stop();  break;
	}
}

void wrecord_init()
{
	gui_wrecord *w = gui_allocT(gui_wrecord);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = wrecord_action;
	w->ac.conf_mp3q = ~0U;
	w->ac.conf_vorbisq = ~0U;
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
		w->wnd_pos = NULL;

		wrecord_ui_from_conf();

		if (gd->recording_track)
			w->bstart.enable(0);
	}

	w->wnd.show(1);
	w->wnd.present();
}
