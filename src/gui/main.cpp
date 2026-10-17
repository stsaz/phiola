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
	ffui_trayiconxx	tray;
#ifdef FF_WIN
	ffui_paned pntop;
#endif

	ffui_icon ico_play, ico_pause;

	struct {
		phi_timer timer;
		uint cmd, n, pos;
		uint gui_ver, cur_ver;
		uint n_total, n_actual;
		uint timer_set :1;
	} redraw;

	char *vlist_col;
	char *wnd_pos;
	uint columns_hidden; // hidden columns bitarray
	uint ready;
};

static ffui_icon& res_icon_load(ffui_icon *ico, const char *name, uint resource_id);

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
	_(tray),
#ifdef FF_WIN
	_(pntop),
#endif
	FFUI_LDR_CTL_END
};
#undef _

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

static void vlist_col_from_conf(uint *width) {
	gui_wmain *m = gg->wmain;
	xxstr w(m->vlist_col);
	uint i = 0;
	while (w.len) {
		xxstr s;
		w.split_by(' ', &s, &w);
		uint n = s.uint16(0);
		if (n)
			width[i] = n;
		i++;
	}
}

/** Write column widths to string */
static char* vlist_col_write() {
	gui_wmain *m = gg->wmain;
	xxvec v;
	ffui_viewcolxx vc = {};
	vc.width(0);
	for (uint i = 0;  i != _H_LAST;  i++) {
		uint w = m->vlist.column(i, &vc).width();
		v.add_f("%u ", w);
	}
	char *r = v.strz();
	v.reset();
	return r;
}

/** Set columns width from a string value list */
static void conf_vlist_col() {
	gui_wmain *m = gg->wmain;
	if (!m->vlist_col) return;

	uint width[_H_LAST] = {};
	vlist_col_from_conf(width);

	ffui_viewcolxx vc = {};
	for (uint i = 0;  i < _H_LAST;  i++) {
		if (width[i]) {
			vc.width(width[i]);
			m->vlist.column(i, vc);
		}
	}

	ffmem_free(m->vlist_col);
	m->vlist_col = NULL;
}

/** Set column visibility */
static void vlist_cols_showhide(uint flags) {
	gui_wmain *m = gg->wmain;
	if (flags && !m->columns_hidden) return; // All columns shown

	ffui_viewcolxx vc = {};
	for (uint i = 0;  i < _H_LAST;  i++) {
		bool show = !(m->columns_hidden & (1U << i));
		if (show && flags)
			continue;

#ifdef FF_WIN
		vc.width(0);
		uint w = m->vlist.column(i, &vc).width();
		if (w && show)
			continue; // Preserve the current width
		ffui_viewcol_setwidth(&vc.vc, (show) ? 100 : 0);
#else
		ffui_viewcol_show(&vc.vc, show);
#endif

		m->vlist.column(i, vc);
	}
}

// Note: 'struct gui{ columns_hidden }' is more logical, but currently there's no conf context for global gui settings
uint wmain_cols_visible() { return ~gg->wmain->columns_hidden & ((1U << _H_LAST) - 1); }
void wmain_cols_visible_set(uint mask) {
	gg->wmain->columns_hidden = (mask)
		? ~mask & ((1U << _H_LAST) - 1)
		: 0; // Prevent showing empty view
	vlist_cols_showhide(0);
}

#define O(m)  (void*)FF_OFF(struct gui_wmain, m)
const ffarg wmain_args[] = {
	{ "cols_hidden",'u',	O(columns_hidden) },
	{ "vlist.col",	'=s',	O(vlist_col) },
	{ "wmain.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

void wmain_userconf_write(ffconfw *cw)
{
	gui_wmain *m = gg->wmain;

	ffmem_free(m->vlist_col);
	m->vlist_col = vlist_col_write(); // Note: win32: original width values for hidden columns are lost

	m->wnd_pos = wnd_pos_sz(&m->wnd);
	ffarg_write_conf(cw, wmain_args, m);
	ffmem_free(m->wnd_pos);
	ffmem_free(m->vlist_col);
}

/** Set status bar text.
Thread: any */
void wmain_status(const char *fmt, ...)
{
	gui_wmain *m = gg->wmain;
	va_list va;
	va_start(va, fmt);
	char buf[1000];
	ffsz_formatv(buf, sizeof(buf), fmt, va);
	m->stbar.text(0, buf);
	va_end(va);
}

/** Handle event.
Thread: any */
void wmain_event(uint event)
{
	gui_wmain *m = gg->wmain;
	switch (event) {
	case GUI_EV_PLAY_STOPPED:
	case GUI_EV_PLAY_PAUSED:
	case GUI_EV_PLAY_PLAYING: {
		ffui_icon *ico;
		if (event == GUI_EV_PLAY_PLAYING)
			ico = &res_icon_load(&m->ico_pause, "pause", 7);
		else
			ico = &res_icon_load(&m->ico_play, "play", 3);
		m->bpause.icon(*ico);
		m->stbar.text(0, (event == GUI_EV_PLAY_PAUSED) ? "Paused" : "");
		break;
	}

	case GUI_EV_REC_STARTED:
	case GUI_EV_REC_DONE: {
		const char *s[][2] = {
			{ "Recording...", "[REC]" },
			{ "Recording complete", "" },
		};
		uint i = (event == GUI_EV_REC_DONE);
		m->stbar.text(0, s[i][0]);
		m->stbar.text(1, s[i][1]);
		break;
	}
	}
}

static ffui_icon& res_icon_load(ffui_icon *ico, const char *name, uint resource_id)
{
	if (!ffui_icon_valid(ico)) {
#ifdef FF_WIN
		ffui_icon_load_res(ico, GetModuleHandleW(L"gui.dll"), xxwstr_buf<100>().utow(xxstr_buf<100>().zfmt("#%u", resource_id)), 0, 0);
#else
		ffui_icon_load(ico, xxstr_buf<4096>().zfmt("%S/mod/gui/%s.png", &core->conf.root, name));
#endif
		PHI_ASSERT(ffui_icon_valid(ico));
	}
	return *ico;
}

int wmain_ready()
{
	gui_wmain *m = gg->wmain;
	return (m && m->ready);
}

void wmain_track_new(void *param)
{
	gui_wmain *m = gg->wmain;
	struct gui_track_info *ti = (struct gui_track_info*)param;

	if (ti->q == gd->q_selected && !gd->q_filtered) {
		// Redraw rows in the currently visible list

		if (ti->index_old != ~0U)
			m->vlist.update(ti->index_old, 0);

		if (ti->index_new != ~0U) {
			m->vlist.update(ti->index_new, 0);
			if (gd->conf.auto_select)
				m->vlist.select(ti->index_new);
		}
	}

	wmain_event(GUI_EV_PLAY_PLAYING);
	m->tpos.range((ti->duration_sec != ~0U) ? ti->duration_sec : 0);

	m->wnd.title(ti->buf);
}

void wmain_track_close(void *param)
{
	gui_wmain *m = gg->wmain;
	struct gui_track_info *ti = (struct gui_track_info*)param;

	if (ti->q == gd->q_selected && !gd->q_filtered
		&& ti->index_old != ~0U) {
		m->vlist.update(ti->index_old, 0);
	}

	m->wnd.title("phiola");
	m->lpos.text("");
	m->tpos.range(0);
	wmain_event(GUI_EV_PLAY_STOPPED);
}

void wmain_track_update(void *param)
{
	struct gui_track_info *ti = (struct gui_track_info*)param;
	gui_wmain *m = gg->wmain;

	m->tpos.set(ti->pos_sec);

	char buf[256];
	if (ti->duration_sec == ~0U) {
		ffsz_format(buf, sizeof(buf), "%u:%02u / --"
			, ti->pos_sec / 60, ti->pos_sec % 60);
	} else {
		ffsz_format(buf, sizeof(buf), "%u:%02u / %u:%02u"
			, ti->pos_sec / 60, ti->pos_sec % 60
			, ti->duration_sec / 60, ti->duration_sec % 60);
	}
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
	fflock_lock((fflock*)&qe->lock);
	core->metaif->set(&qe->meta, FFSTR_Z("_phi_dur"), FFSTR_Z(progress), PHI_META_REPLACE);
	fflock_unlock((fflock*)&qe->lock);
	if (gd->tab_conversion) {
		int idx = gd->queue->index(t->qent);
		m->vlist.update(idx, 0);
	}
}

/** Thread: worker */
void wmain_conv_track_close(phi_track *t)
{
	const char *status = (!t->error) ? "Done" : "ERROR";
	conv_track_update(t, status);
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

static void list_display(ffui_viewxx_disp *disp)
{
#ifdef FF_WIN
	if (!(disp->mask & LVIF_TEXT))
		return;

#else
	gui_wmain *m = gg->wmain;
	if (disp->paint_begin) {
		m->redraw.cur_ver = list_vis_version();
		return;
	} else if (disp->paint_end) {
		FFINT_WRITEONCE(m->redraw.gui_ver, m->redraw.cur_ver);
		return;
	}
#endif

	uint i = disp->index(), sub = disp->subindex();

	// gui_wmain *m = gg->wmain;
	xxstr_buf<1000> buf;
	ffstr *val = NULL, s;
	struct phi_queue_entry *qe = list_vis_qe_ref(i);
	if (!qe)
		return;

	fflock_lock((fflock*)&qe->lock); // core thread may read or write `conf.meta` at this moment
	phi_meta *meta = &qe->meta;

	switch (sub) {
	case H_INDEX:
		buf.zfmt("%s%u", (qe == gd->qe_active) ? "> " : "", i + 1);
		val = &buf;
		break;

	case H_FN:
		s = FFSTR_Z(qe->url);
		val = &s;
		break;

	default:
		if (!core->metaif->find(meta, FFSTR_Z(list_colname[sub]), &s, PHI_META_PRIVATE))
			val = &s;
	}

	switch (sub) {
	case H_TITLE:
		if (!val || !val->len) {
			if (url_checkz(qe->url))
				ffstr_setz(&s, qe->url); // use URL as a title
			else
				ffpath_split3_str(FFSTR_Z(qe->url), NULL, &s, NULL); // use filename as a title
			val = &s;
		}
		break;

	case H_DUR:
		if (!val && qe->length_sec) {
			buf.zfmt("%u:%02u", qe->length_sec / 60, qe->length_sec % 60);
			core->metaif->set(meta, FFSTR_Z("_phi_dur"), buf, 0);
			val = &buf;
		}
		break;
	}

	if (val) {
		disp->text(*val);
	}

	fflock_unlock((fflock*)&qe->lock);
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

	case 'm':
		m->vlist.update(pos, 0);
		break;

	case 'u':
		wmain_list_draw(n, 1);  break;
	}
}

static void list_update_delayed(void *param)
{
	gui_wmain *m = gg->wmain;
	m->redraw.n_actual++;
	list_update(m->redraw.cmd, m->redraw.n, m->redraw.pos);
	m->redraw.timer_set = 0;
}

/** A queue is created/deleted/modified.
Thread: core */
static void q_on_change(phi_queue_id q, uint flags, uint pos)
{
	gui_wmain *m = gg->wmain;

	if (q == gd->q_filtered
		|| !gd->lists_load_done)
		return;

	uint cmd = (flags & 0xff);

	switch (cmd) {
	case 'm':
	case 'a':
	case 'r':
	case 'u': {
		if (q != gd->q_selected)
			break; // an inactive list is changed
		m->redraw.n_total++;
		uint n = gd->queue->count(q);
		uint update = m->redraw.timer_set;

#ifdef FF_WIN
		m->redraw.cmd = cmd;
		m->redraw.pos = pos;
		// Note: currently there's no way to determine if the list-view painting is complete

#else
		uint q_ver = gd->queue->conf(q)->version;
		update |= (q_ver - 1 != FFINT_READONCE(m->redraw.gui_ver));
		if (!update) {
			m->redraw.n_actual++;
			list_update(cmd, n, pos);
			break;
		}
#endif

		if (update) {
			// Multiple changes within short time period: redraw the whole list
			m->redraw.cmd = 'u';
			m->redraw.pos = 0;
		}
		m->redraw.n = n;
		if (!m->redraw.timer_set) {
			m->redraw.timer_set = 1;
			core->timer(0, &m->redraw.timer, -50, list_update_delayed, NULL);
		}
		break;
	}

	case 'c':
		m->vlist.clear();  break;

	case 'n':
		if (gd->filtering)
			break; // a filtered list is created
		list_created(q);
		break;

	case 'd':
		list_deleted(q);  break;

	case '.':
		if (q == gd->q_convert)
			gui_event(GUI_EV_CONV_DONE);
		break;
	}
}

static void vol_set(uint id)
{
	gui_wmain *m = gg->wmain;
	uint v = gd->conf.volume;

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

/** Rename list at position 'i'.
Thread: core */
void wmain_list_rename(uint i, const char *name)
{
	gui_wmain *m = gg->wmain;
	m->tabs.text(i, name);
}

void wmain_lists_load(struct lists_load_data *d)
{
	struct gui_wmain *m = gg->wmain;
	for (uint i = 0;  i < d->n;  i++) {
		m->tabs.add(list_name_i(i));
	}
	m->tabs.select(d->sel);
	ffmem_free(d);
}

/** Add the files chosen by user */
static void list_add_choose()
{
	struct gui_wmain *m = gg->wmain;
	char *fn;
	if (!(fn = ffui_dlg_open(&gg->dlg, &m->wnd)))
		return;

	xxvec names;
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
	xxvec name;
	name.acquire(list_cur_path());
	if (!(fn = ffui_dlg_save(&gg->dlg, &m->wnd, name.sz(), name.len)))
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
	case A_FILE_SHOWDIR:
		gui_core_task_slice(file_dir_show, m->vlist.selected());  break;

	case A_FILE_DEL:
		if (!gd->tab_conversion)
			gui_core_task_slice(file_del, m->vlist.selected());
		break;

	case A_TRAY_MIN:
	case A_TRAY_CLICK: {
		bool tray = (id == A_TRAY_MIN);
		m->tray.show(tray);
		m->wnd.show(!tray);
		if (tray)
			gui_cmd(GUI_CMD_HIDE);
		break;
	}

	case A_QUIT:
		m->wnd.close();  break;

// Playback:
#ifdef FF_WIN
	case IDOK:
#endif
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

	case A_REPEAT_TOGGLE:
		gg->mplay.check(A_REPEAT_TOGGLE, !gd->conf.repeat);
		gui_core_task_uint(ctl_action, id);
		break;

	case A_RANDOM_TOGGLE:
		gg->mplay.check(A_RANDOM_TOGGLE, !gd->conf.random);
		gui_core_task_uint(ctl_action, id);
		break;

// List:
	case A_LIST_CHANGE:
		list_changed(m->tabs.changed());  break;

	case A_LIST_ADD_FILE:
		list_add_choose();  break;

	case A_LIST_ADDTONEXT:
		gui_core_task_slice(list_add_to_next, m->vlist.selected());  break;

	case A_LIST_SAVE:
		list_save_choose_filename();  break;

	case A_LIST_META_READ:
		gui_core_task(list_meta_read);  break;

	case A_LIST_REMOVE:
		gui_core_task_slice(list_remove, m->vlist.selected());  break;

	case A_LIST_SCROLL_TO_CUR:
		if (!gd->q_filtered)
			m->vlist.scroll_index(gd->cursor);
		break;

	case A_LIST_AUTOSELECT:
		gd->conf.auto_select = !gd->conf.auto_select;
		gg->mlist.check(A_LIST_AUTOSELECT, gd->conf.auto_select);
		wmain_status("Auto Select Current: %s", (gd->conf.auto_select) ? "On" : "Off");
		break;

	case A_LIST_DISPLAY:
		list_display((ffui_viewxx_disp*)m->vlist.dispinfo_item);  break;

// Record:
	case A_RECORD_START_STOP:
		wrecord_start_stop();  break;

// Convert:
	case A_CONVERT_POS_START:
	case A_CONVERT_POS_END:
		wconvert_set(id, ffmax((int)m->tpos.get() - 1, 0));  break;

// Misc:
	case A_CLOSE:
		gui_quit();

#ifdef FF_WIN
		if (m->wnd.close_forced) {
			gui_core_task_uint(gui_stop, 1);
			// Wait until process's main() exits, but note that the UI thread is hanging now
			ffthread_sleep(3000);
		}
#endif
		break;

	case A_FILE_DRAGDROP:
		on_drop_files();  break;

	case A_FILE_INFO:
	case A_FILE_RENAME:
	case A_MLIB_SHOW:
	case A_SETTINGS_SHOW:
	case A_LIST_RENAME:
	case A_LIST_ADD:
	case A_LIST_FILTER:
	case A_RECORD_SHOW:
	case A_CONVERT_SHOW:
	case A_ABOUT_SHOW:
		if (gui_dlg_load())
			return;

		switch (id) {
		case A_FILE_INFO:
			if ((i = m->vlist.selected_first()) >= 0)
				winfo_show(i);
			break;
		case A_FILE_RENAME:
			if ((i = m->vlist.selected_first()) >= 0)
				wrename_show(i);
			break;
		case A_MLIB_SHOW: wmlib_show();  break;
		case A_SETTINGS_SHOW: wsettings_show();  break;

		case A_LIST_RENAME: wrename_show_list();  break;
		case A_LIST_ADD: wlistadd_show();  break;
		case A_LIST_FILTER: wlistfilter_show();  break;

		case A_RECORD_SHOW: wrecord_show();  break;

		case A_CONVERT_SHOW: {
			ffslice items = {};
			if (!gd->tab_conversion)
				items = m->vlist.selected();
			wconvert_show(items);
			break;
		}

		case A_ABOUT_SHOW: wabout_show();  break;
		}
		break;

	default:
		gui_core_task_uint(ctl_action, id);
	}
}

static void wmain_init()
{
	gui_wmain *m = gui_allocT(gui_wmain);
	gg->wmain = m;
#ifdef FF_WIN
	m->wnd.top = 1;
	m->wnd.manual_close = 1;
#else
	m->vlist.paint_notify = 1;
#endif

	static const struct ffui_wnd_keymap kmap[] = {
#ifdef FF_WIN
		{ APPCOMMAND_MEDIA_PLAY_PAUSE,    A_PLAYPAUSE },
		{ APPCOMMAND_MEDIA_STOP,          A_STOP },
		{ APPCOMMAND_MEDIA_NEXTTRACK,     A_NEXT },
		{ APPCOMMAND_MEDIA_PREVIOUSTRACK, A_PREV },
#else
		{ GDK_KEY_AudioPlay,	A_PLAYPAUSE },
		{ GDK_KEY_AudioStop,	A_STOP },
		{ GDK_KEY_AudioNext,	A_NEXT },
		{ GDK_KEY_AudioPrev,	A_PREV },
#endif
	};
	ffui_wnd_keymap(&m->wnd, kmap, FF_COUNT(kmap));

	m->wnd.on_action = wmain_action;
	m->wnd.onclose_id = A_CLOSE;
	m->vlist.dispinfo_id = A_LIST_DISPLAY;
}

void wmain_show()
{
	gui_wmain *m = gg->wmain;

	conf_vlist_col();
	vlist_cols_showhide(1);

	if (m->wnd_pos)
		conf_wnd_pos_read(&m->wnd, FFSTR_Z(m->wnd_pos));
	ffmem_free(m->wnd_pos);
	m->wnd_pos = NULL;

	m->tvol.set(gd->conf.volume);
	m->wnd.tab_auto_switch(&m->tabs);
	m->wnd.show(1);
	wmain_list_draw(gd->queue->count(gd->q_selected), 0);
	drag_drop_init();

	gd->queue->on_change(q_on_change);
	gui_core_task(lists_load);

	if (gd->conf.auto_select)
		gg->mlist.check(A_LIST_AUTOSELECT, 1);
	if (gd->conf.repeat)
		gg->mplay.check(A_REPEAT_TOGGLE, 1);
	if (gd->conf.random)
		gg->mplay.check(A_RANDOM_TOGGLE, 1);

	m->ready = 1;
}

static void wmain_fin()
{
	gui_wmain *m = gg->wmain;
	core->timer(0, &m->redraw.timer, 0, NULL, NULL);
	dbglog("%s  paint-events: %u/%u"
		, __func__, m->redraw.n_actual, m->redraw.n_total);
}

void wmain_cmd(uint cmd)
{
	switch (cmd) {
	case GUI_CMD_INIT:
		wmain_init();  break;
	case GUI_CMD_FIN:
		wmain_fin();  break;
	}
}
