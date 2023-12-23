/** phiola: GUI: Main window
2023, Simon Zolin */

#include <gui/gui.h>
#include <track.h>
#include <util/util.h>
#include <util/util.hpp>

#define VOLUME_MAX 125

struct gui_wmain {
	ffui_windowxx	wnd;
	ffui_menu		mm;
	ffui_buttonxx	bpause, bstop, bprev, bnext;
	ffui_labelxx	lpos;
	ffui_trackbarxx	tvol, tpos;
	ffui_tabxx		tabs;
	ffui_viewxx		vlist;
	ffui_statusbarxx stbar;
#ifdef FF_WIN
	ffui_paned pntop;
#endif

	ffui_icon ico_play, ico_pause;
	struct phi_queue_entry *qe_active;

	phi_timer tmr_redraw;
	uint redraw_n, redraw_offset;

	char *vlist_col;
	char *wnd_pos;
	uint tvol_val;
	uint ready;
};

#define _(m) FFUI_LDR_CTL(gui_wmain, m)
FF_EXTERN const ffui_ldr_ctl wmain_ctls[] = {
	_(wnd),
	_(mm),
	_(bpause), _(bstop), _(bprev), _(bnext),
	_(lpos),
	_(tvol),
	_(tpos),
	_(tabs),
	_(vlist),
	_(stbar),
#ifdef FF_WIN
	_(pntop),
#endif
	FFUI_LDR_CTL_END
};
#undef _

enum LIST_HDR {
	H_INDEX,
	H_ARTIST,
	H_TITLE,
	H_DUR,
	H_INFO,
	H_DATE,
	H_ALBUM,
	H_FN,
	_H_LAST,
};

static const char list_colname[][10] = {
	"", // H_INDEX
	"artist", // H_ARTIST
	"title", // H_TITLE
	"_phi_dur", // H_DUR
	"_phi_info", // H_INFO
	"date", // H_DATE
	"album", // H_ALBUM
	"", // H_FN
};

static void conf_vlist_col(ffui_viewxx &v, ffstrxx val)
{
	ffui_viewcolxx vc = {};
	uint i = 0;
	while (val.len) {
		ffstrxx s;
		val.split(' ', &s, &val);
		uint width;
		if (!s.matchf("%u", &width)) {
			vc.width(width);
			v.column(i, vc);
		}
		i++;
	}
}

#define O(m)  (void*)FF_OFF(struct gui_wmain, m)
const ffarg wmain_args[] = {
	{ "tvol",		'u',	O(tvol_val) },
	{ "vlist.col",	'=s',	O(vlist_col) },
	{ "wmain.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

void wmain_userconf_write(ffconfw *cw)
{
	gui_wmain *m = gg->wmain;
	ffconfw_add2u(cw, "tvol", m->tvol.get());

	ffvecxx v = {};
	ffui_viewcolxx vc = {};
	vc.width(0);
	for (uint i = 0;  i != _H_LAST;  i++) {
		v.addf("%u ", m->vlist.column(i, &vc).width());
	}
	ffconfw_add2s(cw, "vlist.col", v.str());

	conf_wnd_pos_write(cw, "wmain.pos", &m->wnd);
}

/** Set status bar text */
void wmain_status(const char *fmt, ...)
{
	gui_wmain *m = gg->wmain;
	va_list va;
	va_start(va, fmt);
	char buf[1000];
	ffsz_formatv(buf, sizeof(buf), fmt, va);
	m->stbar.text(buf);
	va_end(va);
}

static ffui_icon& res_icon_load(ffui_icon *ico, const char *name, uint resource_id)
{
	if (!ffui_icon_valid(ico)) {
#ifdef FF_WIN
		ffui_icon_load_res(ico, GetModuleHandleW(L"gui.dll"), ffwstrxx_buf<100>().utow(ffstrxx_buf<100>().zfmt("#%u", resource_id)), 0, 0);
#else
		ffui_icon_load(ico, ffstrxx_buf<4096>().zfmt("%S/mod/gui/%s.png", &core->conf.root, name));
#endif
		PHI_ASSERT(ffui_icon_valid(ico));
	}
	return *ico;
}

void wmain_status_id(uint id)
{
	gui_wmain *m = gg->wmain;
	ffui_icon *ico = NULL;
	switch (id) {
	case ST_STOPPED:
	case ST_PAUSED:
		ico = &res_icon_load(&m->ico_play, "play", 3);  break;

	case ST_UNPAUSED:
		ico = &res_icon_load(&m->ico_pause, "pause", 7);  break;
	}

	m->bpause.icon(*ico);
	wmain_status((id == ST_PAUSED) ? "Paused" : "");
}

static const char pcm_fmtstr[][8] = {
	"float32",
	"float64",
	"int16",
	"int24",
	"int24-4",
	"int32",
	"int8",
};
static const ushort pcm_fmt[] = {
	PHI_PCM_FLOAT32,
	PHI_PCM_FLOAT64,
	PHI_PCM_16,
	PHI_PCM_24,
	PHI_PCM_24_4,
	PHI_PCM_32,
	PHI_PCM_8,
};

static const char* pcm_fmt_str(uint fmt)
{
	int r = ffarrint16_find(pcm_fmt, FF_COUNT(pcm_fmt), fmt);
	if (r < 0)
		return "";
	return pcm_fmtstr[r];
}

static const char _pcm_channelstr[][10] = {
	"mono", "stereo",
	"3-channel", "4-channel", "5-channel",
	"5.1", "6.1", "7.1"
};

static const char* pcm_channelstr(uint channels)
{
	return _pcm_channelstr[ffmin(channels - 1, FF_COUNT(_pcm_channelstr) - 1)];
}

/** Thread: worker */
int wmain_track_new(phi_track *t, uint time_total)
{
	gui_wmain *m = gg->wmain;
	if (!m || !m->ready) return -1;

	wmain_status_id(ST_UNPAUSED);

	struct phi_queue_entry *qe = (struct phi_queue_entry*)t->qent;
	char buf[1000];
	ffsz_format(buf, sizeof(buf), "%u kbps, %s, %u Hz, %s, %s"
		, (t->audio.bitrate + 500) / 1000
		, t->audio.decoder
		, t->audio.format.rate
		, pcm_fmt_str(t->audio.format.format)
		, pcm_channelstr(t->audio.format.channels));
	gd->metaif->set(&t->meta, FFSTR_Z("_phi_info"), FFSTR_Z(buf), 0);

	m->tpos.range(time_total);
	qe->length_msec = time_total * 1000;

	void *qe_active = m->qe_active;
	m->qe_active = qe;

	int idx;
	if (qe_active && -1 != (idx = gd->queue->index(qe_active)) && !gd->q_filtered)
		m->vlist.update(idx, 0);

	if (-1 != (idx = gd->queue->index(qe))) {
		if (!gd->q_filtered) { // 'idx' is the position within the original list, not filtered list
			m->vlist.update(idx, 0);
			if (gd->auto_select)
				m->vlist.select(idx);
		}
		gd->cursor = idx;
	}

	ffstr artist = {}, title = {};
	gd->metaif->find(&t->meta, FFSTR_Z("artist"), &artist, 0);
	gd->metaif->find(&t->meta, FFSTR_Z("title"), &title, 0);
	if (!title.len)
		ffpath_split3_str(FFSTR_Z(qe->conf.ifile.name), NULL, &title, NULL); // use filename as a title

	ffsz_format(buf, sizeof(buf), "%S - %S - phiola", &artist, &title);
	m->wnd.title(buf);
	return 0;
}

/** Thread: worker */
void wmain_track_close()
{
	gui_wmain *m = gg->wmain;
	if (!m || !m->ready) return;

	if (m->qe_active != NULL) {
		int idx = gd->queue->index(m->qe_active);
		m->qe_active = NULL;
		if (idx >= 0)
			m->vlist.update(idx, 0);
	}

	m->wnd.title("phiola");
	m->lpos.text("");
	m->tpos.range(0);
	wmain_status_id(ST_STOPPED);
}

/** Thread: worker */
void wmain_track_update(uint time_cur, uint time_total)
{
	gui_wmain *m = gg->wmain;
	if (!m || !m->ready) return;

	m->tpos.set(time_cur);

	char buf[256];
	ffsz_format(buf, sizeof(buf), "%u:%02u / %u:%02u"
		, time_cur / 60, time_cur % 60
		, time_total / 60, time_total % 60);
	m->lpos.text(buf);
}


/** Thread: worker */
void wmain_conv_track_new(phi_track *t, uint time_total)
{
}

/** Thread: worker */
static void conv_track_update(phi_track *t, const char *progress)
{
	gui_wmain *m = gg->wmain;
	struct phi_queue_entry *qe = (struct phi_queue_entry*)t->qent;
	gd->metaif->set(&qe->conf.meta, FFSTR_Z("_phi_dur"), FFSTR_Z(progress), PHI_META_REPLACE);
	if (gd->tab_conversion) {
		int idx = gd->queue->index(t->qent);
		m->vlist.update(idx, 0);
	}
}

/** Thread: worker */
void wmain_conv_track_close(phi_track *t)
{
	conv_track_update(t, "Done");
}

/** Thread: worker */
void wmain_conv_track_update(phi_track *t, uint time_cur, uint time_total)
{
	char buf[256];
	ffsz_format(buf, sizeof(buf), "%u:%02u / %u:%02u"
		, time_cur / 60, time_cur % 60
		, time_total / 60, time_total % 60);
	conv_track_update(t, buf);
}


static void list_display(ffui_view_disp *disp)
{
#ifdef FF_WIN
	if (!(disp->mask & LVIF_TEXT))
		return;
	uint i = disp->iItem, sub = disp->iSubItem;
#else
	uint i = disp->idx, sub = disp->sub;
#endif

	gui_wmain *m = gg->wmain;
	ffstrxx_buf<1000> buf;
	ffstr *val = NULL, s;
	struct phi_queue_entry *qe = gd->queue->ref(list_id_visible(), i);
	if (!qe)
		return;

	switch (sub) {
	case H_INDEX:
		buf.zfmt("%s%u", (qe == m->qe_active) ? "> " : "", i + 1);
		val = &buf;
		break;

	case H_FN:
		s = FFSTR_Z(qe->conf.ifile.name);
		val = &s;
		break;

	default:
		if (!gd->metaif->find(&qe->conf.meta, FFSTR_Z(list_colname[sub]), &s, PHI_META_PRIVATE))
			val = &s;
	}

	switch (sub) {
	case H_TITLE:
		if (!val || !val->len) {
			ffpath_split3_str(FFSTR_Z(qe->conf.ifile.name), NULL, &s, NULL); // use filename as a title
			val = &s;
		}
		break;

	case H_DUR:
		if (!val && qe->length_msec != 0) {
			uint sec = qe->length_msec / 1000;
			buf.zfmt("%u:%02u", sec / 60, sec % 60);
			gd->metaif->set(&qe->conf.meta, FFSTR_Z("_phi_dur"), buf, 0);
			val = &buf;
		}
		break;
	}

	if (val) {
#ifdef FF_WIN
		ffui_view_dispinfo_settext(disp, val->ptr, val->len);
#else
		disp->text.len = ffmem_ncopy(disp->text.ptr, disp->text.len, val->ptr, val->len);
#endif
	}

	gd->queue->unref(qe);
}

/** A new list at position 'i' is added.
Thread: worker */
uint wmain_list_add(const char *name, uint i)
{
	gui_wmain *m = gg->wmain;
	m->tabs.add(name);
	m->tabs.select(i);
	uint scroll_vpos = m->vlist.scroll_vert();
	m->vlist.clear();
	return scroll_vpos;
}

/** The list at position 'i' is deleted.
Thread: worker */
void wmain_list_delete(uint i)
{
	gui_wmain *m = gg->wmain;
	uint new_index = (!i) ? 0 : i-1;
	gd->current_scroll_vpos = m->vlist.scroll_vert();
	list_select(new_index);
	m->tabs.del(i);
	m->tabs.select(new_index);
}

static void list_update(uint cmd, uint n, uint pos)
{
	gui_wmain *m = gg->wmain;
	switch (cmd) {
	case 'a':
		m->vlist.length(n, 0);
		m->vlist.update(pos, 1);
		break;

	case 'r':
		m->vlist.length(n, 0);
		m->vlist.update(pos, -1);
#ifdef FF_LINUX
		if (pos < n)
			m->vlist.update(pos, 0);
#endif
		break;
	}
}

#ifdef FF_WIN

static void list_update_delayed(void *param)
{
	gui_wmain *m = gg->wmain;
	list_update((size_t)param, m->redraw_n, m->redraw_offset);
}

static void list_update_schedule(uint cmd, uint n, uint pos)
{
	gui_wmain *m = gg->wmain;
	switch (cmd) {
	case 'a':
	case 'r':
		// Note: view.length() here is very slow
		m->redraw_n = n;
		m->redraw_offset = pos;
		core->timer(0, &m->tmr_redraw, -50, list_update_delayed, (void*)(size_t)cmd);
		break;

	default:
		core->timer(0, &m->tmr_redraw, 0, NULL, NULL);
	}
}

#else // linux:

#define list_update_schedule  list_update

#endif

/** A queue is created/deleted/modified.
Thread: worker */
static void q_on_change(phi_queue_id q, uint flags, uint pos)
{
	gui_wmain *m = gg->wmain;

	if (q == gd->q_filtered)
		return;

	uint q_len = 0;

	switch (flags & 0xff) {
	case 'a':
	case 'r':
		q_len = gd->queue->count(q);
		// fallthrough

	case 'c':
		if (q != gd->q_selected)
			return; // an inactive list is changed
		break;

	case 'n':
		if (gd->filtering)
			return; // a filtered list is created
		break;
	}

	list_update_schedule(flags & 0xff, q_len, pos);

	switch (flags & 0xff) {
	case 'c':
		m->vlist.clear();  break;

	case 'n':
		list_created(q);  break;

	case 'd':
		list_deleted(q);  break;
	}
}

static void vol_set(uint id)
{
	gui_wmain *m = gg->wmain;
	uint v = gd->volume;

	switch (id) {
	case A_VOL:
		v = m->tvol.get();  goto apply;

	case A_VOLUP:
		v = ffmin(v + 5, VOLUME_MAX);  break;

	case A_VOLDOWN:
		v = ffmax((int)v - 5, 0);  break;
	}

	m->tvol.set(v);

apply:
	volume_set(v);
	wmain_status("Volume: %.02FdB", gd->gain_db);
	gui_core_task(ctl_volume);
}

#ifdef FF_WIN

static void wmain_on_drop_files(ffui_window *wnd, ffui_fdrop *df)
{
	ffvec buf = {};
	const char *fn;
	while (NULL != (fn = ffui_fdrop_next(df))) {
		ffvec_addfmt(&buf, "%s\n", fn);
	}
	gui_core_task_data(gui_dragdrop, *(ffstr*)&buf);
	ffvec_null(&buf);
}

static void on_drop_files(){}

static void drag_drop_init()
{
	gg->wmain->wnd.on_dropfiles = wmain_on_drop_files;
	ffui_fdrop_accept(&gg->wmain->wnd, 1);
}

#else

static void on_drop_files()
{
	ffstr d = {};
	ffstr_dupstr(&d, &gg->wmain->vlist.drop_data);
	gui_core_task_data(gui_dragdrop, d);
}

static void drag_drop_init()
{
	gg->wmain->vlist.drag_drop_init(A_FILE_DRAGDROP);
}

#endif

/** Draw/redraw the listing.
Thread: gui, worker */
void wmain_list_draw(uint n, uint flags)
{
	gui_wmain *m = gg->wmain;

	m->vlist.length(n, 1);
#ifdef FF_LINUX
	if (flags == 1)
		m->vlist.clear();
	m->vlist.update(0, n);
#endif
}

/** A new tab is selected */
static void list_changed(uint i)
{
	gui_wmain *m = gg->wmain;
	gd->current_scroll_vpos = m->vlist.scroll_vert();
	gui_core_task_uint(list_select, i);
}

static void list_scroll(uint scroll_vpos)
{
	struct gui_wmain *m = gg->wmain;
	m->vlist.scroll_vert(scroll_vpos);
}

/** Switched to a new list */
void wmain_list_select(uint n, uint scroll_vpos)
{
	wmain_list_draw(n, 1);
	gui_task_uint(list_scroll, scroll_vpos);
}

/** Add the files chosen by user */
static void list_add_choose()
{
	struct gui_wmain *m = gg->wmain;
	char *fn;
	if (!(fn = ffui_dlg_open(&gg->dlg, &m->wnd)))
		return;

	ffvecxx names;
	for (;;) {
		*names.push<char*>() = ffsz_dup(fn);
		if (!(fn = ffui_dlg_nextname(&gg->dlg)))
			break;
	}
	gui_core_task_slice(list_add_multi, names.slice());
	names.reset();
}

/** Get the output file name from user */
static void list_save_choose_filename()
{
	gui_wmain *m = gg->wmain;
	char *fn;
	ffstr name = FFSTR_INITZ("Playlist.m3u8");
	if (!(fn = ffui_dlg_save(&gg->dlg, &m->wnd, name.ptr, name.len)))
		return;

	char *fn2 = ffsz_dup(fn);
	gui_core_task_ptr(list_save, fn2);
}

static void wmain_action(ffui_window *wnd, int id)
{
	gui_wmain *m = gg->wmain;
	int i;
	dbglog("%s cmd:%u", __func__, id);

	switch (id) {
// File:
	case A_FILE_INFO:
		if ((i = m->vlist.selected_first()) >= 0)
			winfo_show(1, i);
		break;

	case A_FILE_SHOWDIR:
		gui_core_task_slice(file_dir_show, m->vlist.selected());  break;

	case A_FILE_DEL:
		if (!gd->tab_conversion)
			gui_core_task_slice(file_del, m->vlist.selected());
		break;

	case A_SETTINGS_SHOW:
		wsettings_show(1);  break;

	case A_QUIT:
		m->wnd.close();  break;

// Playback:
	case A_PLAY:
		if (!gd->tab_conversion && (i = m->vlist.focused()) >= 0)
			gui_core_task_uint(ctl_play, i);
		break;

	case A_VOL:
	case A_VOLUP:
	case A_VOLDOWN:
		vol_set(id);  break;

	case A_SEEK:
		gd->seek_pos_sec = m->tpos.get();
		gui_core_task_uint(ctl_action, A_SEEK);
		break;

// List:
	case A_LIST_CHANGE:
		list_changed(m->tabs.changed());  break;

	case A_LIST_ADD_FILE:
		list_add_choose();  break;

	case A_LIST_ADD:
		wlistadd_show(1);  break;

	case A_LIST_ADDTONEXT:
		gui_core_task_slice(list_add_to_next, m->vlist.selected());  break;

	case A_LIST_FILTER:
		wlistfilter_show(1);  break;

	case A_LIST_SAVE:
		list_save_choose_filename();  break;

	case A_LIST_REMOVE:
		gui_core_task_slice(list_remove, m->vlist.selected());  break;

	case A_LIST_SCROLL_TO_CUR:
		if (!gd->q_filtered)
			m->vlist.scroll_index(gd->cursor);
		break;

	case A_LIST_AUTOSELECT:
		gd->auto_select = !gd->auto_select;
		wmain_status("Auto Select Current: %s", (gd->auto_select) ? "On" : "Off");
		break;

	case A_LIST_DISPLAY:
#ifdef FF_WIN
		list_display(m->vlist.dispinfo_item);
#else
		list_display(&m->vlist.disp);
#endif
		break;

// Record:
	case A_RECORD_SHOW:
		wrecord_show(1);  break;

	case A_RECORD_START_STOP:
		wrecord_start_stop();  break;

// Convert:
	case A_CONVERT_SHOW: {
		ffslice items = {};
		if (!gd->tab_conversion)
			items = m->vlist.selected();
		wconvert_show(1, items);
		break;
	}

	case A_CONVERT_POS_START:
	case A_CONVERT_POS_END:
		wconvert_set(id, m->tpos.get());  break;

// Misc:
	case A_ABOUT_THEME_SWITCH:
		theme_switch();  break;

	case A_ABOUT_SHOW:
		wabout_show(1);  break;

	case A_CLOSE:
		gui_quit();  break;

	case A_FILE_DRAGDROP:
		on_drop_files();  break;

	default:
		gui_core_task_uint(ctl_action, id);
	}
}

void wmain_init()
{
	gui_wmain *m = ffmem_new(gui_wmain);
	gg->wmain = m;
#ifdef FF_WIN
	m->wnd.top = 1;
	m->wnd.manual_close = 1;
#endif
	m->wnd.on_action = wmain_action;
	m->wnd.onclose_id = A_CLOSE;
	m->vlist.dispinfo_id = A_LIST_DISPLAY;
	m->tvol_val = 100;
}

void wmain_show()
{
	gui_wmain *m = gg->wmain;

	if (m->vlist_col)
		conf_vlist_col(m->vlist, m->vlist_col);
	ffmem_free(m->vlist_col);

	if (m->wnd_pos)
		conf_wnd_pos_read(&m->wnd, FFSTR_Z(m->wnd_pos));
	ffmem_free(m->wnd_pos);

	m->tvol.set(m->tvol_val);
	m->tabs.add("Playlist 1");
	m->wnd.show(1);
	wmain_list_draw(gd->queue->count(gd->q_selected), 0);
	drag_drop_init();

	gd->queue->on_change(q_on_change);
	gui_core_task(lists_load);
	volume_set(m->tvol_val);

	m->ready = 1;
}

void wmain_fin()
{
	gui_wmain *m = gg->wmain;
	core->timer(0, &m->tmr_redraw, 0, NULL, NULL);
}
