/** GUI/GTK+
2019, Simon Zolin
*/

#include <FFOS/error.h>
#include "gtk.h"
#include "dialog.h"
#include "view.h"
#include "window.h"
#include <ffbase/vector.h>
#include <ffbase/atomic.h>
#include <ffbase/lock.h>
#include <FFOS/thread.h>

ffuint64 _ffui_thd_id;

void ffui_run()
{
	_ffui_thd_id = ffthread_curid();
	gtk_main();
}

#define sig_disable(h, func, udata) \
	g_signal_handlers_disconnect_matched(h, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, 0, G_CALLBACK(func), udata)


void _ffui_menu_activate(GtkWidget *mi, gpointer udata)
{
	GtkWidget *parent_menu = mi;
	ffui_wnd *wnd = NULL;
	while (wnd == NULL) {
		parent_menu = gtk_widget_get_parent(parent_menu);
		wnd = g_object_get_data(G_OBJECT(parent_menu), "ffdata");
	}
	uint id = (ffsize)udata;
	wnd->on_action(wnd, id);
}

void _ffui_btn_clicked(GtkWidget *widget, gpointer udata)
{
	ffui_btn *b = udata;
	b->wnd->on_action(b->wnd, b->action_id);
}

void _ffui_checkbox_clicked(GtkWidget *widget, gpointer udata)
{
	ffui_checkbox *cb = udata;
	cb->wnd->on_action(cb->wnd, cb->action_id);
}

void _ffui_edit_changed(GtkEditable *editable, gpointer udata)
{
	ffui_edit *e = udata;
	if (e->change_id != 0)
		e->wnd->on_action(e->wnd, e->change_id);
}

void _ffui_trk_value_changed(GtkWidget *widget, gpointer udata)
{
	ffui_trkbar *t = udata;
	if (t->scroll_id != 0)
		t->wnd->on_action(t->wnd, t->scroll_id);
}

void _ffui_tab_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer udata)
{
	ffui_tab *t = udata;
	t->changed_index = page_num;
	t->wnd->on_action(t->wnd, t->change_id);
}

int ffui_tab_create(ffui_tab *t, ffui_wnd *parent)
{
	if (NULL == (t->h = gtk_notebook_new()))
		return -1;
	t->wnd = parent;
	gtk_box_pack_start(GTK_BOX(parent->vbox), t->h, /*expand=*/0, /*fill=*/0, /*padding=*/0);
	g_signal_connect(t->h, "switch-page", G_CALLBACK(_ffui_tab_switch_page), t);
	return 0;
}

void ffui_tab_ins(ffui_tab *t, int idx, const char *textz)
{
	GtkWidget *label = gtk_label_new(textz);
	GtkWidget *child = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	g_signal_handlers_block_by_func(t->h, G_CALLBACK(_ffui_tab_switch_page), t);
	gtk_notebook_insert_page(GTK_NOTEBOOK(t->h), child, label, idx);
	gtk_widget_show_all(t->h);
	g_signal_handlers_unblock_by_func(t->h, G_CALLBACK(_ffui_tab_switch_page), t);
}

void ffui_tab_setactive(ffui_tab *t, int idx)
{
	g_signal_handlers_block_by_func(t->h, G_CALLBACK(_ffui_tab_switch_page), t);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(t->h), idx);
	g_signal_handlers_unblock_by_func(t->h, G_CALLBACK(_ffui_tab_switch_page), t);
}


// LISTVIEW
static void _ffui_view_row_activated(void *a, GtkTreePath *path, void *c, gpointer udata)
{
	_ffui_log("udata: %p", udata);
	ffui_view *v = udata;
	v->path = path;
	v->wnd->on_action(v->wnd, v->dblclick_id);
	v->path = NULL;
}

static void _ffui_view_drag_data_received(GtkWidget *wgt, GdkDragContext *context, int x, int y,
	GtkSelectionData *seldata, guint info, guint time, gpointer userdata)
{
	gint len;
	const void *ptr = gtk_selection_data_get_data_with_length(seldata, &len);
	_ffui_log("seldata:[%u] %*s", len, (ffsize)len, ptr);

	ffui_view *v = userdata;
	ffstr_set(&v->drop_data, ptr, len);
	v->wnd->on_action(v->wnd, v->dropfile_id);
	ffstr_null(&v->drop_data);
}

/** Replace each "%XX" escape sequence in URL string with a byte value
Return N of bytes written
 <0 if not enough space */
static int url_unescape(char *buf, ffsize cap, ffstr url)
{
	char *d = url.ptr, *end = url.ptr + url.len, *p = buf, *ebuf = buf + cap;

	while (d != end) {
		ffssize r = ffs_findchar(d, end - d, '%');
		if (r < 0)
			r = end - d;
		if (r > ebuf - p)
			return -1;
		p = ffmem_copy(p, d, r);
		d += r;

		if (d == end)
			break;

		if (d+3 > end || d[0] != '%')
			return -1;
		int h = ffchar_tohex(d[1]);
		int l = ffchar_tohex(d[2]);
		if (h < 0 || l < 0)
			return -1;
		if (p == ebuf)
			return -1;
		*p++ = (h<<4) | l;
		d += 3;
	}

	return p - buf;
}

int ffui_fdrop_next(ffvec *fn, ffstr *dropdata)
{
	ffstr ln;
	while (dropdata->len != 0) {
		ffstr_splitby(dropdata, '\n', &ln, dropdata);
		ffstr_rskipchar1(&ln, '\r');
		if (!ffstr_matchz(&ln, "file://"))
			continue;
		ffstr_shift(&ln, FFS_LEN("file://"));

		if (NULL == ffvec_realloc(fn, ln.len, 1))
			return -1;
		int r = url_unescape(fn->ptr, fn->cap, ln);
		if (r < 0)
			return -1;
		fn->len = r;
		if (fn->len == 0)
			return -1;
		return 0;
	}
	return -1;
}

static void _ffui_view_cell_edited(GtkCellRendererText *cell, gchar *path_string, gchar *text, gpointer udata)
{
	ffui_view *v = udata;
	GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
	int *ii = gtk_tree_path_get_indices(path);
	v->edited.idx = ii[0];
	v->edited.new_text = text;
	v->wnd->on_action(v->wnd, v->edit_id);
	v->edited.idx = 0;
	v->edited.new_text = NULL;
}

static gboolean _ffui_view_button_press_event(GtkWidget *w, GdkEventButton *ev, gpointer udata)
{
	if (ev->type == GDK_BUTTON_PRESS && ev->button == 3 /*right mouse button*/) {
		ffui_view *v = udata;
		if (v->popup_menu != NULL) {

			GtkTreePath *path;
			if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(v->h), ev->x, ev->y, &path, NULL, NULL, NULL))
				return 0;
			void *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(v->h));
			gtk_tree_selection_select_path(sel, path);
			gtk_tree_path_free(path);

			guint32 t = gdk_event_get_time((GdkEvent*)ev);
			gtk_menu_popup(GTK_MENU(v->popup_menu->h), NULL, NULL, NULL, NULL, ev->button, t);
			return 1;
		}
	}
	return 0;
}

int ffui_view_create(ffui_view *v, ffui_wnd *parent)
{
	v->h = gtk_tree_view_new();
	g_signal_connect(v->h, "row-activated", G_CALLBACK(_ffui_view_row_activated), v);
	v->wnd = parent;
	v->rend = gtk_cell_renderer_text_new();
	g_signal_connect(v->rend, "edited", (GCallback)_ffui_view_cell_edited, v);
	GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scroll), v->h);
	gtk_box_pack_start(GTK_BOX(parent->vbox), scroll, /*expand=*/1, /*fill=*/1, /*padding=*/0);
	g_signal_connect(v->h, "button-press-event", (GCallback)_ffui_view_button_press_event, v);
	return 0;
}

static void view_prepare(ffui_view *v)
{
	uint ncol = gtk_tree_view_get_n_columns(GTK_TREE_VIEW(v->h));
	GType *types = ffmem_alloc(ncol * sizeof(GType));
	for (uint i = 0;  i != ncol;  i++) {
		types[i] = G_TYPE_STRING;
	}
	v->store = (void*)gtk_list_store_newv(ncol, types);
	ffmem_free(types);
	gtk_tree_view_set_model(GTK_TREE_VIEW(v->h), v->store);
	g_object_unref(v->store);
}

void ffui_view_dragdrop(ffui_view *v, uint action_id)
{
	if (v->store == NULL)
		view_prepare(v);

	static const GtkTargetEntry ents[] = {
		{ "text/uri-list", GTK_TARGET_OTHER_APP, 0 }
	};
	gtk_drag_dest_set(v->h, GTK_DEST_DEFAULT_ALL, ents, FF_COUNT(ents), GDK_ACTION_COPY);
	g_signal_connect(v->h, "drag_data_received", G_CALLBACK(_ffui_view_drag_data_received), v);
	v->dropfile_id = action_id;
}

void ffui_view_ins(ffui_view *v, int pos, ffui_viewitem *it)
{
	_ffui_log("pos:%d", pos);
	if (v->store == NULL)
		view_prepare(v);

	GtkTreeIter iter;
	if (pos == -1) {
		it->idx = ffui_view_nitems(v);
		gtk_list_store_append(GTK_LIST_STORE(v->store), &iter);
	} else {
		it->idx = pos;
		gtk_list_store_insert(GTK_LIST_STORE(v->store), &iter, pos);
	}
	gtk_list_store_set(GTK_LIST_STORE(v->store), &iter, 0, it->text, -1);
	ffui_view_itemreset(it);
}

void ffui_view_setdata(ffui_view *v, uint first, int delta)
{
	if (v->store == NULL)
		view_prepare(v);

	FF_ASSERT(v->dispinfo_id != 0);
	uint cols = ffui_view_ncols(v);
	if (cols == 0)
		return;

	uint rows = ffui_view_nitems(v);
	int n = first + delta;
	if (delta == 0 && rows != 0)
		n++; // redraw the item

	_ffui_log("first:%u  delta:%d  rows:%u", first, delta, rows);

	if (first > rows)
		return;

	char buf[1024];
	ffui_viewitem it = {};
	for (uint i = first;  (int)i < n;  i++) {

		v->disp.idx = i;

		ffstr_set(&v->disp.text, buf, sizeof(buf) - 1);
		buf[0] = '\0';
		v->disp.sub = 0;
		v->wnd->on_action(v->wnd, v->dispinfo_id);

		it.idx = i;
		buf[v->disp.text.len] = '\0';
		it.text = (char*)buf;
		int ins = 0;
		if (i >= rows) {
			ffui_view_ins(v, -1, &it);
			rows++;
			ins = 1;
		} else if (delta == 0) {
			ffui_view_set(v, 0, &it); // redraw
		} else {
			ffui_view_ins(v, i, &it);
			rows++;
			ins = 1;
		}
		_ffui_log("idx:%u  text:%s", i, buf);

		for (uint c = 1;  c != cols;  c++) {
			ffstr_set(&v->disp.text, buf, sizeof(buf) - 1);
			buf[0] = '\0';
			v->disp.sub = c;
			v->wnd->on_action(v->wnd, v->dispinfo_id);
			if (ins && buf[0] == '\0')
				continue;

			it.idx = i;
			buf[v->disp.text.len] = '\0';
			it.text = (char*)buf;
			ffui_view_set(v, c, &it);
			_ffui_log("idx:%u  text:%s", i, buf);
		}
	}

	int i = first;
	while (i > (int)n) {
		it.idx = i;
		ffui_view_rm(v, &it);
		_ffui_log("removed idx:%u", i);
		i--;
	}
}


// STATUSBAR

int ffui_stbar_create(ffui_ctl *sb, ffui_wnd *parent)
{
	sb->h = gtk_statusbar_new();
	gtk_box_pack_end(GTK_BOX(parent->vbox), sb->h, /*expand=*/0, /*fill=*/0, /*padding=*/0);
	return 0;
}


// TRAYICON
static void _ffui_tray_activate(GtkWidget *mi, gpointer udata)
{
	ffui_trayicon *t = udata;
	t->wnd->on_action(t->wnd, t->lclick_id);
}

int ffui_tray_create(ffui_trayicon *t, ffui_wnd *wnd)
{
	t->h = (void*)gtk_status_icon_new();
	t->wnd = wnd;
	g_signal_connect(t->h, "activate", G_CALLBACK(_ffui_tray_activate), t);
	ffui_tray_show(t, 0);
	return 0;
}


// WINDOW
static void _ffui_wnd_onclose(void *a, void *b, gpointer udata)
{
	ffui_wnd *wnd = udata;
	wnd->on_action(wnd, wnd->onclose_id);
	if (wnd->hide_on_close) {
		ffui_show(wnd, 0);
		return;
	}
}

int ffui_wnd_create(ffui_wnd *w)
{
	w->uid = FFUI_UID_WINDOW;
	w->h = (void*)gtk_window_new(GTK_WINDOW_TOPLEVEL);
	w->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(w->h), w->vbox);
	g_object_set_data(G_OBJECT(w->h), "ffdata", w);
	g_signal_connect(w->h, "delete-event", G_CALLBACK(_ffui_wnd_onclose), w);
	return 0;
}

static gboolean _ffui_wnd_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_KEY_Escape) {
		_ffui_wnd_onclose(widget, event, data);
		return 1;
	}
	return 0;
}

void ffui_wnd_setpopup(ffui_wnd *w, ffui_wnd *parent)
{
	gtk_window_set_transient_for(w->h, parent->h);
	g_signal_connect(w->h, "key_press_event", G_CALLBACK(_ffui_wnd_key_press_event), w);
}

static const ushort _ffui_ikeys[] = {
	GDK_KEY_BackSpace,
	GDK_KEY_Break,
	GDK_KEY_Delete,
	GDK_KEY_Down,
	GDK_KEY_End,
	GDK_KEY_Return,
	GDK_KEY_Escape,
	GDK_KEY_F1,
	GDK_KEY_F10,
	GDK_KEY_F11,
	GDK_KEY_F12,
	GDK_KEY_F2,
	GDK_KEY_F3,
	GDK_KEY_F4,
	GDK_KEY_F5,
	GDK_KEY_F6,
	GDK_KEY_F7,
	GDK_KEY_F8,
	GDK_KEY_F9,
	GDK_KEY_Home,
	GDK_KEY_Insert,
	GDK_KEY_Left,
	GDK_KEY_Right,
	GDK_KEY_space,
	GDK_KEY_Tab,
	GDK_KEY_Up,
};

static const char *const _ffui_keystr[] = {
	"backspace",
	"break",
	"delete",
	"down",
	"end",
	"enter",
	"escape",
	"f1",
	"f10",
	"f11",
	"f12",
	"f2",
	"f3",
	"f4",
	"f5",
	"f6",
	"f7",
	"f8",
	"f9",
	"home",
	"insert",
	"left",
	"right",
	"space",
	"tab",
	"up",
};

ffui_hotkey ffui_hotkey_parse(const char *s, ffsize len)
{
	int r = 0;
	enum {
		fctrl = GDK_CONTROL_MASK << 16,
		fshift = GDK_SHIFT_MASK << 16,
		falt = GDK_MOD1_MASK << 16,
	};

	ffstr v, ss = FFSTR_INITN(s, len);
	if (!ss.len)
		goto fail;

	while (ss.len) {
		ffstr_splitby(&ss, '+', &v, &ss);

		int f;
		if (ffstr_ieqcz(&v, "ctrl"))
			f = fctrl;
		else if (ffstr_ieqcz(&v, "alt"))
			f = falt;
		else if (ffstr_ieqcz(&v, "shift"))
			f = fshift;
		else {
			if (ss.len)
				goto fail; // 2nd key
			break;
		}

		if (r & f)
			goto fail; // duplicate key modifier
		r |= f;
	}

	if (v.len == 1) {
		int c = v.ptr[0];
		if (((c|0x20) >= 'a' && (c|0x20) <= 'z')
			|| (c >= '0' && c <= '9')
			|| c == '[' || c == ']'
			|| c == '`'
			|| c == '/' || c == '\\')
			r |= c;
		else
			goto fail; // unknown key

	} else {
		ffssize ikey = ffszarr_ifindsorted(_ffui_keystr, FF_COUNT(_ffui_keystr), v.ptr, v.len);
		if (ikey == -1)
			goto fail; // unknown key
		r |= _ffui_ikeys[ikey];
	}

	return r;

fail:
	return 0;
}

int ffui_wnd_hotkeys(ffui_wnd *w, const ffui_wnd_hotkey *hotkeys, ffsize n)
{
	GtkAccelGroup *ag = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(w->h), ag);

	for (uint i = 0;  i != n;  i++) {
		gtk_widget_add_accelerator(hotkeys[i].h, "activate", ag
			, ffui_hotkey_key(hotkeys[i].hk), ffui_hotkey_mod(hotkeys[i].hk), GTK_ACCEL_VISIBLE);
	}

	return 0;
}


// MESSAGE LOOP

struct cmd {
	ffui_handler func;
	void *udata;
	uint id;
	uint ref;
};

static gboolean _ffui_thd_func(gpointer data)
{
	struct cmd *c = data;
	_ffui_log("func:%p  udata:%p", c->func, c->udata);
	c->func(c->udata);
	if (c->ref != 0) {
		ffcpu_fence_release();
		FFINT_WRITEONCE(c->ref, 0);
	} else
		ffmem_free(c);
	return 0;
}

void ffui_thd_post(ffui_handler func, void *udata)
{
	_ffui_log("func:%p  udata:%p", func, udata);

	struct cmd *c = ffmem_new(struct cmd);
	c->func = func;
	c->udata = udata;

	if (0 != gdk_threads_add_idle(_ffui_thd_func, c)) {
	}
}

struct cmd_send {
	void *ctl;
	void *udata;
	uint id;
	uint ref;
};

static gboolean _ffui_send_handler(gpointer data)
{
	struct cmd_send *c = data;
	switch ((enum FFUI_MSG)c->id) {

	case FFUI_QUITLOOP:
		ffui_quitloop();  break;


	case FFUI_CTL_ENABLE:
		gtk_widget_set_sensitive(((ffui_ctl*)c->ctl)->h, (ffsize)c->udata);  break;


	case FFUI_LBL_SETTEXT:
		ffui_lbl_settextz((ffui_label*)c->ctl, c->udata);  break;

	case FFUI_EDIT_GETTEXT:
		ffui_edit_textstr((ffui_edit*)c->ctl, c->udata);  break;

	case FFUI_STBAR_SETTEXT:
		ffui_stbar_settextz((ffui_ctl*)c->ctl, c->udata);  break;


	case FFUI_TEXT_SETTEXT:
		ffui_text_clear((ffui_text*)c->ctl);
		ffui_text_addtextstr((ffui_text*)c->ctl, (ffstr*)c->udata);
		break;

	case FFUI_TEXT_ADDTEXT:
		ffui_text_addtextstr((ffui_text*)c->ctl, (ffstr*)c->udata);  break;


	case FFUI_CHECKBOX_SETTEXTZ:
		ffui_checkbox_settextz((ffui_checkbox*)c->ctl, c->udata);  break;

	case FFUI_CHECKBOX_CHECKED:
		*(ffsize*)c->udata = ffui_checkbox_checked((ffui_checkbox*)c->ctl);  break;


	case FFUI_WND_SETTEXT:
		ffui_wnd_settextz((ffui_wnd*)c->ctl, c->udata);  break;

	case FFUI_WND_SHOW:
		ffui_show((ffui_wnd*)c->ctl, (ffsize)c->udata);  break;


	case FFUI_VIEW_CLEAR:
		ffui_view_clear((ffui_view*)c->ctl);  break;

	case FFUI_VIEW_SCROLLSET:
		ffui_view_scroll_setvert((ffui_view*)c->ctl, (ffsize)c->udata);  break;

	case FFUI_VIEW_SCROLL:
		*(ffsize*)c->udata = ffui_view_scroll_vert((ffui_view*)c->ctl);  break;

	case FFUI_VIEW_SETDATA: {
		uint first = (ffsize)c->udata >> 16;
		uint delta = (ffssize)c->udata & 0xffff;
		ffui_view_setdata((ffui_view*)c->ctl, first, (short)delta);
		break;
	}


	case FFUI_TRK_SETRANGE:
		ffui_trk_setrange((ffui_trkbar*)c->ctl, (ffsize)c->udata);  break;

	case FFUI_TRK_SET:
		ffui_trk_set((ffui_trkbar*)c->ctl, (ffsize)c->udata);  break;


	case FFUI_TAB_INS:
		ffui_tab_append((ffui_tab*)c->ctl, c->udata);  break;

	case FFUI_TAB_SETACTIVE:
		ffui_tab_setactive((ffui_tab*)c->ctl, (ffsize)c->udata);  break;

	case FFUI_TAB_ACTIVE:
		*(ffsize*)c->udata = ffui_tab_active((ffui_tab*)c->ctl);  break;

	case FFUI_TAB_COUNT:
		*(ffsize*)c->udata = ffui_tab_count((ffui_tab*)c->ctl);  break;


	case FFUI_CLIP_SETTEXT: {
		GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
		ffstr *s = c->udata;
		gtk_clipboard_set_text(clip, s->ptr, s->len);
		break;
	}
	}

	if (c->ref != 0) {
		ffcpu_fence_release();
		FFINT_WRITEONCE(c->ref, 0);
	} else {
		ffmem_free(c);
	}
	return 0;
}

static uint quit;

static int post_locked(gboolean (*func)(gpointer), void *udata)
{
	int r = 0;
	if (!FFINT_READONCE(quit))
		r = gdk_threads_add_idle(func, udata);
	return r;
}

ffsize ffui_send(void *ctl, uint id, void *udata)
{
	_ffui_log("ctl:%p  udata:%p  id:%xu", ctl, udata, id);
	struct cmd_send c;
	c.ctl = ctl;
	c.id = id;
	c.udata = udata;
	c.ref = 1;
	ffcpu_fence_release();

	if (ffthread_curid() == _ffui_thd_id) {
		return _ffui_send_handler(&c);
	}

	if (0 != post_locked(&_ffui_send_handler, &c)) {
		ffint_wait_until_equal(&c.ref, 0);
		return (ffsize)c.udata;
	}
	return 0;
}

void ffui_post(void *ctl, uint id, void *udata)
{
	_ffui_log("ctl:%p  udata:%p  id:%xu", ctl, udata, id);
	struct cmd_send *c = ffmem_new(struct cmd_send);
	c->ctl = ctl;
	c->id = id;
	c->udata = udata;
	ffcpu_fence_release();

	if (id == FFUI_QUITLOOP) {
		FFINT_WRITEONCE(quit, 1);
		if (0 == gdk_threads_add_idle(_ffui_send_handler, c))
			ffmem_free(c);
		return;
	}

	if (ffthread_curid() == _ffui_thd_id && id != FFUI_VIEW_SCROLLSET) {
		_ffui_send_handler(c);
		return;
	}

	if (0 == post_locked(&_ffui_send_handler, c))
		ffmem_free(c);
}
