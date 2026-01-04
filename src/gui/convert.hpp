/** phiola: GUI: convert files
2023, Simon Zolin */

#include <util/util.h>

struct gui_wconvert {
	ffui_windowxx		wnd;
	ffui_labelxx		ldir, lname, lext, lfrom, luntil, ltags, laacq, lvorbisq, lopusq, lmp3q;
	ffui_editxx			edir, ename, efrom, euntil, etags, eaacq, evorbisq, eopusq, emp3q;
	ffui_comboboxxx		cbext;
	ffui_checkboxxx		cbcopy, cbkeepdate, cboverwrite;
	ffui_buttonxx		bbrowse, bstart;

	xxstr conf_dir, conf_name, conf_ext;
	char *wnd_pos;
	uint conf_copy;
	uint conf_aacq, conf_vorbisq, conf_opusq, conf_mp3q;
	uint initialized :1;
};

#define _(m)  FFUI_LDR_CTL(gui_wconvert, m)
FF_EXTERN const ffui_ldr_ctl wconvert_ctls[] = {
	_(wnd),
	_(ldir),	_(edir),
	_(lname),	_(ename),
	_(lext),	_(cbext),
	_(bbrowse),
	_(lfrom),	_(efrom),
	_(luntil),	_(euntil),
	_(ltags),	_(etags),
	_(cbcopy),
	_(laacq),	_(eaacq),
	_(lvorbisq),_(evorbisq),
	_(lopusq),	_(eopusq),
	_(lmp3q),	_(emp3q),
	_(cbkeepdate), _(cboverwrite),
	_(bstart),
	FFUI_LDR_CTL_END
};
#undef _

#define O(m)  (void*)FF_OFF(gui_wconvert, m)
const ffarg wconvert_args[] = {
	{ "aacq",	'u',	O(conf_aacq) },
	{ "copy",	'u',	O(conf_copy) },
	{ "dir",	'=S',	O(conf_dir) },
	{ "ext",	'=S',	O(conf_ext) },
	{ "mp3q",	'u',	O(conf_mp3q) },
	{ "name",	'=S',	O(conf_name) },
	{ "opusq",	'u',	O(conf_opusq) },
	{ "vorbisq",'u',	O(conf_vorbisq) },
	{ "wconvert.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

static uint vorbisq_conf(xxstr s)
{
	int n = s.int16(255);
	if (n == 255) {
		errlog("incorrect Vorbis quality '%S'", &s);
		return 0;
	}
	return (n + 1) * 10;
}

static int vorbisq_user(uint n) { return (int)n / 10 - 1; }

static void wconvert_ui_to_conf()
{
	gui_wconvert *c = gg->wconvert;
	c->conf_dir.free();
	c->conf_name.free();
	c->conf_ext.free();
	c->conf_dir = c->edir.text();
	c->conf_name = c->ename.text();
	c->conf_ext = c->cbext.text();

	c->conf_copy = c->cbcopy.checked();

	c->conf_aacq = xxvec(c->eaacq.text()).str().uint32(0);
	c->conf_vorbisq = vorbisq_conf(xxvec(c->evorbisq.text()).str());
	c->conf_opusq = xxvec(c->eopusq.text()).str().uint32(0);
	c->conf_mp3q = xxvec(c->emp3q.text()).str().uint32(~0U);
}

void wconvert_userconf_write(ffconfw *cw)
{
	gui_wconvert *c = gg->wconvert;
	if (c->initialized)
		wconvert_ui_to_conf();
	ffconfw_add2s(cw, "dir", c->conf_dir);
	ffconfw_add2s(cw, "name", c->conf_name);
	ffconfw_add2s(cw, "ext", c->conf_ext);
	ffconfw_add2u(cw, "copy", c->conf_copy);
	ffconfw_add2u(cw, "aacq", c->conf_aacq);
	ffconfw_add2u(cw, "vorbisq", c->conf_vorbisq);
	ffconfw_add2u(cw, "opusq", c->conf_opusq);
	ffconfw_add2u(cw, "mp3q", c->conf_mp3q);

	if (c->initialized)
		conf_wnd_pos_write(cw, "wconvert.pos", &c->wnd);
	else if (c->wnd_pos)
		ffconfw_add2z(cw, "wconvert.pos", c->wnd_pos);
}

static const char out_file_ext[][5] = {
	"m4a",
	"ogg",
	"opus",
	"flac",
	"wav",
	"mp3",
};
static const char out_file_aenc[] = {
	PHI_AC_AAC,
	PHI_AC_VORBIS,
	PHI_AC_OPUS,
	0,
	0,
	0,
};

/** Get file extension index by value */
static int out_file_ext_index(xxstr val)
{
	for (uint i = 0;  i < FF_COUNT(out_file_ext);  i++) {
		if (val.equals_i(out_file_ext[i]))
			return i;
	}
	return -1;
}

static void wconvert_ui_from_conf()
{
	gui_wconvert *c = gg->wconvert;
	c->edir.text((c->conf_dir.len) ? c->conf_dir : "@filepath");
	c->ename.text((c->conf_name.len) ? c->conf_name : "@filename");
	c->conf_dir.free();
	c->conf_name.free();

	uint cbext_index = 0 /*m4a*/;
	for (uint i = 0;  i < FF_COUNT(out_file_ext);  i++) {
		c->cbext.add(out_file_ext[i]);
		if (c->conf_ext.equals(out_file_ext[i]))
			cbext_index = i;
	}
	c->cbext.set(cbext_index);
	c->conf_ext.free();

	c->cbcopy.check(!!c->conf_copy);

	xxstr_buf<100> s;
	c->eaacq.text(s.zfmt("%u", (c->conf_aacq) ? c->conf_aacq : 5));
	c->evorbisq.text(s.zfmt("%d", (c->conf_vorbisq) ? vorbisq_user(c->conf_vorbisq) : 7));
	c->eopusq.text(s.zfmt("%u", (c->conf_opusq) ? c->conf_opusq : 192));
	c->emp3q.text(s.zfmt("%u", (c->conf_mp3q != ~0U) ? c->conf_mp3q : 2));
}

void wconvert_set(int id, uint pos)
{
	gui_wconvert *c = gg->wconvert;
	switch (id) {
	case A_CONVERT_POS_START:
	case A_CONVERT_POS_END:
		if (c->initialized) {
			char buf[100];
			ffsz_format(buf, sizeof(buf), "%u:%02u", pos/60, pos%60);
			if (id == A_CONVERT_POS_START)
				c->efrom.text(buf);
			else
				c->euntil.text(buf);
		}
		break;
	}
}

void wconvert_done()
{
	gui_wconvert *c = gg->wconvert;
	c->bstart.enable(1);
}

static int time_value(ffstr s)
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

static struct phi_track_conf* conv_conf_create()
{
	gui_wconvert *c = gg->wconvert;
	struct phi_track_conf *tc = ffmem_new(struct phi_track_conf);
	wconvert_ui_to_conf();
	tc->seek_msec = time_value(xxvec(c->efrom.text()).str());
	tc->until_msec = time_value(xxvec(c->euntil.text()).str());
	tc->stream_copy = c->conf_copy;

	xxvec tags = c->etags.text();
	xxstr s = tags.str(), name, val;
	while (s.len) {
		s.split_by(';', &name, &s);
		name.split_by('=', &name, &val);
		if (name.len)
			core->metaif->set(&tc->meta, name, val, 0);
	}

	int i = out_file_ext_index(c->conf_ext);
	switch (out_file_aenc[i]) {
	case PHI_AC_AAC:
		tc->aac.quality = c->conf_aacq;  break;
	case PHI_AC_OPUS:
		tc->opus.bitrate = c->conf_opusq;  break;
	case PHI_AC_VORBIS:
		tc->vorbis.quality = c->conf_vorbisq;  break;
	case PHI_AC_MP3:
		tc->mp3.quality = (c->conf_mp3q != ~0U) ? c->conf_mp3q + 1 : 0;  break;
	}

	tc->ifile.preserve_date = c->cbkeepdate.checked();
	tc->ofile.overwrite = c->cboverwrite.checked();
	tc->ofile.name = ffsz_allocfmt("%S/%S.%S", &c->conf_dir, &c->conf_name, &c->conf_ext);

	tc->cross_worker_assign = 1;
	return tc;
}

static void wconvert_browse()
{
	gui_wconvert *c = gg->wconvert;

	xxvec v;
	v.add_f("%S/%S.%S"
		, &xxvec(c->edir.text()).str()
		, &xxvec(c->ename.text()).str()
		, &xxvec(c->cbext.text()).str());

	char *fn;
	if (!(fn = ffui_dlg_save(&gg->dlg, &c->wnd, (char*)v.ptr, v.len)))
		return;

	ffstr path, name, ext;
	ffpath_split3_str(FFSTR_Z(fn), &path, &name, &ext);

	c->edir.text(path);
	c->ename.text(name);

	int i = out_file_ext_index(ext);
	if (i >= 0)
		c->cbext.set(i);
}

static void wconvert_action(ffui_window *wnd, int id)
{
	gui_wconvert *c = gg->wconvert;
	switch (id) {

	case A_CONVERT_BROWSE:
		wconvert_browse();  break;

	case A_CONVERT_START: {
		c->bstart.enable(0);
		struct phi_track_conf *conf = conv_conf_create();
		if (conf)
			gui_core_task_ptr(convert_begin, conf);
		else
			wconvert_done();
		break;
	}
	}
}

void wconvert_init()
{
	gui_wconvert *c = ffmem_new(gui_wconvert);
	c->wnd.hide_on_close = 1;
	c->wnd.on_action = wconvert_action;
	c->conf_mp3q = ~0U;
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

		if (c->wnd_pos)
			conf_wnd_pos_read(&c->wnd, FFSTR_Z(c->wnd_pos));
		ffmem_free(c->wnd_pos);

		wconvert_ui_from_conf();
	}

	gui_core_task_slice(convert_add, items);
	c->wnd.show(1);
}
