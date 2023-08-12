/** GUI based on GTK+.
2019, Simon Zolin
*/

#pragma once
#include <ffbase/string.h>
#include <ffbase/stringz.h>
#include <gtk/gtk.h>

#ifdef FFGUI_DEBUG
	#include <FFOS/std.h>
	#define _ffui_log fflog
#else
	#define _ffui_log(...)
#endif

static inline void ffui_init()
{
	int argc = 0;
	char **argv = NULL;
	gtk_init(&argc, &argv);
}

#define ffui_uninit()


// ICON
// CONTROL
// MENU
// BUTTON
// CHECKBOX
// LABEL
// IMAGE
// EDITBOX
// TEXT
// TRACKBAR
// TAB
// STATUSBAR
// TRAYICON
// MESSAGE LOOP


typedef struct ffui_pos {
	int x, y
		, cx, cy;
} ffui_pos;


// ICON
typedef struct ffui_icon {
	GdkPixbuf *ico;
} ffui_icon;

static inline int ffui_icon_load(ffui_icon *ico, const char *filename)
{
	ico->ico = gdk_pixbuf_new_from_file(filename, NULL);
	return (ico->ico == NULL);
}

static inline int ffui_icon_loadimg(ffui_icon *ico, const char *filename, uint cx, uint cy, uint flags)
{
	ico->ico = gdk_pixbuf_new_from_file_at_scale(filename, cx, cy, 0, NULL);
	return (ico->ico == NULL);
}



// CONTROL

enum FFUI_UID {
	FFUI_UID_WINDOW = 1,
	FFUI_UID_TRACKBAR,
};

typedef struct ffui_wnd ffui_wnd;

#define _FFUI_CTL_MEMBERS \
	GtkWidget *h; \
	enum FFUI_UID uid; \
	ffui_wnd *wnd;

typedef struct ffui_ctl {
	_FFUI_CTL_MEMBERS
} ffui_ctl;

typedef struct ffui_view ffui_view;

static inline void ffui_show(void *c, uint show)
{
	if (show)
		gtk_widget_show_all(((ffui_ctl*)c)->h);
	else
		gtk_widget_hide(((ffui_ctl*)c)->h);
}

#define ffui_ctl_destroy(c)  gtk_widget_destroy(((ffui_ctl*)c)->h)

#define ffui_setposrect(ctl, r) \
	gtk_widget_set_size_request((ctl)->h, (r)->cx, (r)->cy)


// MENU
typedef struct ffui_menu {
	_FFUI_CTL_MEMBERS
} ffui_menu;

static inline int ffui_menu_createmain(ffui_menu *m)
{
	m->h = gtk_menu_bar_new();
	return (m->h == NULL);
}

static inline int ffui_menu_create(ffui_menu *m)
{
	m->h = gtk_menu_new();
	return (m->h == NULL);
}

#define ffui_menu_new(text)  gtk_menu_item_new_with_mnemonic(text)
#define ffui_menu_newsep()  gtk_separator_menu_item_new()

static inline void ffui_menu_setsubmenu(void *mi, ffui_menu *sub, ffui_wnd *wnd)
{
	gtk_menu_item_set_submenu((GtkMenuItem*)mi, sub->h);
	g_object_set_data(G_OBJECT(sub->h), "ffdata", wnd);
}

FF_EXTERN void _ffui_menu_activate(GtkWidget *mi, gpointer udata);
static inline void ffui_menu_setcmd(void *mi, uint id)
{
	g_signal_connect(mi, "activate", (GCallback)_ffui_menu_activate, (void*)(ffsize)id);
}

static inline void ffui_menu_ins(ffui_menu *m, void *mi, int pos)
{
	gtk_menu_shell_insert(GTK_MENU_SHELL(m->h), GTK_WIDGET(mi), pos);
}


// BUTTON
typedef struct ffui_btn {
	_FFUI_CTL_MEMBERS
	uint action_id;
} ffui_btn;

FF_EXTERN void _ffui_btn_clicked(GtkWidget *widget, gpointer udata);
static inline int ffui_btn_create(ffui_btn *b, ffui_wnd *parent)
{
	b->h = gtk_button_new();
	b->wnd = parent;
	g_signal_connect(b->h, "clicked", (GCallback)_ffui_btn_clicked, b);
	return 0;
}

static inline void ffui_btn_settextz(ffui_btn *b, const char *textz)
{
	gtk_button_set_label(GTK_BUTTON(b->h), textz);
}
static inline void ffui_btn_settextstr(ffui_btn *b, const ffstr *text)
{
	char *sz = ffsz_dupstr(text);
	gtk_button_set_label(GTK_BUTTON(b->h), sz);
	ffmem_free(sz);
}

static inline void ffui_btn_seticon(ffui_btn *b, ffui_icon *ico)
{
	GtkWidget *img = gtk_image_new();
	gtk_image_set_from_pixbuf(GTK_IMAGE(img), ico->ico);
	gtk_button_set_image(GTK_BUTTON(b->h), img);
}


// CHECKBOX
typedef struct ffui_checkbox {
	_FFUI_CTL_MEMBERS
	uint action_id;
} ffui_checkbox;

void _ffui_checkbox_clicked(GtkWidget *widget, gpointer udata);
static inline int ffui_checkbox_create(ffui_checkbox *cb, ffui_wnd *parent)
{
	cb->h = gtk_check_button_new();
	cb->wnd = parent;
	g_signal_connect(cb->h, "clicked", (GCallback)_ffui_checkbox_clicked, cb);
	return 0;
}
static inline void ffui_checkbox_settextz(ffui_checkbox *cb, const char *textz)
{
	gtk_button_set_label(GTK_BUTTON(cb->h), textz);
}
static inline void ffui_checkbox_settextstr(ffui_checkbox *cb, const ffstr *text)
{
	char *sz = ffsz_dupstr(text);
	gtk_button_set_label(GTK_BUTTON(cb->h), sz);
	ffmem_free(sz);
}
static inline int ffui_checkbox_checked(ffui_checkbox *cb)
{
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb->h));
}
static inline void ffui_checkbox_check(ffui_checkbox *cb, int val)
{
	g_signal_handlers_block_by_func(cb->h, (void*)_ffui_checkbox_clicked, cb);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb->h), val);
	g_signal_handlers_unblock_by_func(cb->h, (void*)_ffui_checkbox_clicked, cb);
}


// LABEL
typedef struct ffui_label {
	_FFUI_CTL_MEMBERS
} ffui_label;

static inline int ffui_lbl_create(ffui_label *l, ffui_wnd *parent)
{
	l->h = gtk_label_new("");
	l->wnd = parent;
	return 0;
}

#define ffui_lbl_settextz(l, text)  gtk_label_set_text(GTK_LABEL((l)->h), text)
static inline void ffui_lbl_settext(ffui_label *l, const char *text, ffsize len)
{
	char *sz = ffsz_dupn(text, len);
	ffui_lbl_settextz(l, sz);
	ffmem_free(sz);
}
#define ffui_lbl_settextstr(l, str)  ffui_lbl_settext(l, (str)->ptr, (str)->len)


// IMAGE
typedef struct ffui_image {
	_FFUI_CTL_MEMBERS
} ffui_image;

static inline int ffui_image_create(ffui_image *c, ffui_wnd *parent)
{
	c->h = gtk_image_new();
	c->wnd = parent;
	return 0;
}

static inline void ffui_image_setfile(ffui_image *c, const char *fn)
{
	gtk_image_set_from_file(GTK_IMAGE(c->h), fn);
}

static inline void ffui_image_seticon(ffui_image *c, ffui_icon *ico)
{
	gtk_image_set_from_pixbuf(GTK_IMAGE(c->h), ico->ico);
}


// EDITBOX
typedef struct ffui_edit {
	_FFUI_CTL_MEMBERS
	uint change_id;
} ffui_edit;

FF_EXTERN void _ffui_edit_changed(GtkEditable *editable, gpointer udata);
static inline int ffui_edit_create(ffui_edit *e, ffui_wnd *parent)
{
	e->h = gtk_entry_new();
	e->wnd = parent;
	g_signal_connect(e->h, "changed", (GCallback)_ffui_edit_changed, e);
	return 0;
}

#define ffui_edit_settextz(e, text)  gtk_entry_set_text(GTK_ENTRY((e)->h), text)
static inline void ffui_edit_settext(ffui_edit *e, const char *text, ffsize len)
{
	char *sz = ffsz_dupn(text, len);
	ffui_edit_settextz(e, sz);
	ffmem_free(sz);
}
#define ffui_edit_settextstr(e, str)  ffui_edit_settext(e, (str)->ptr, (str)->len)

static inline void ffui_edit_textstr(ffui_edit *e, ffstr *s)
{
	const gchar *sz = gtk_entry_get_text(GTK_ENTRY(e->h));
	ffsize len = ffsz_len(sz);
	char *p = ffsz_dupn(sz, len);
	FF_ASSERT(s->ptr == NULL);
	ffstr_set(s, p, len);
}

#define ffui_edit_sel(e, start, end) \
	gtk_editable_select_region(GTK_EDITABLE((e)->h), start, end);


// TEXT
typedef struct ffui_text {
	_FFUI_CTL_MEMBERS
} ffui_text;

static inline int ffui_text_create(ffui_text *t, ffui_wnd *parent)
{
	t->h = gtk_text_view_new();
	t->wnd = parent;
	return 0;
}

static inline void ffui_text_addtext(ffui_text *t, const char *text, ffsize len)
{
	GtkTextBuffer *gbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t->h));
	GtkTextIter end;
	gtk_text_buffer_get_end_iter(gbuf, &end);
	gtk_text_buffer_insert(gbuf, &end, text, len);
}
#define ffui_text_addtextz(t, text)  ffui_text_addtext(t, text, ffsz_len(text))
#define ffui_text_addtextstr(t, str)  ffui_text_addtext(t, (str)->ptr, (str)->len)

static inline void ffui_text_clear(ffui_text *t)
{
	GtkTextBuffer *gbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t->h));
	GtkTextIter start, end;
	gtk_text_buffer_get_start_iter(gbuf, &start);
	gtk_text_buffer_get_end_iter(gbuf, &end);
	gtk_text_buffer_delete(gbuf, &start, &end);
}

static inline void ffui_text_scroll(ffui_text *t, int pos)
{
	GtkTextBuffer *gbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t->h));
	GtkTextIter iter;
	if (pos == -1)
		gtk_text_buffer_get_end_iter(gbuf, &iter);
	else
		return;
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(t->h), &iter, 0.0, 0, 0.0, 1.0);
}

/**
mode: GTK_WRAP_NONE GTK_WRAP_CHAR GTK_WRAP_WORD GTK_WRAP_WORD_CHAR*/
#define ffui_text_setwrapmode(t, mode)  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW((t)->h), mode)

#define ffui_text_setmonospace(t, mono)  gtk_text_view_set_monospace(GTK_TEXT_VIEW((t)->h), mono)


// TRACKBAR
typedef struct ffui_trkbar {
	_FFUI_CTL_MEMBERS
	uint scroll_id;
} ffui_trkbar;

FF_EXTERN void _ffui_trk_value_changed(GtkWidget *widget, gpointer udata);
static inline int ffui_trk_create(ffui_trkbar *t, ffui_wnd *parent)
{
	t->uid = FFUI_UID_TRACKBAR;
	t->h = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
	gtk_scale_set_draw_value(GTK_SCALE(t->h), 0);
	t->wnd = parent;
	g_signal_connect(t->h, "value-changed", (GCallback)_ffui_trk_value_changed, t);
	return 0;
}

static inline void ffui_trk_setrange(ffui_trkbar *t, uint max)
{
	g_signal_handlers_block_by_func(t->h, (GClosure*)_ffui_trk_value_changed, t);
	gtk_range_set_range(GTK_RANGE((t)->h), 0, max);
	g_signal_handlers_unblock_by_func(t->h, (GClosure*)_ffui_trk_value_changed, t);
}

static inline void ffui_trk_set(ffui_trkbar *t, uint val)
{
	g_signal_handlers_block_by_func(t->h, (GClosure*)_ffui_trk_value_changed, t);
	gtk_range_set_value(GTK_RANGE(t->h), val);
	g_signal_handlers_unblock_by_func(t->h, (GClosure*)_ffui_trk_value_changed, t);
}

#define ffui_trk_val(t)  gtk_range_get_value(GTK_RANGE((t)->h))


// TAB
typedef struct ffui_tab {
	_FFUI_CTL_MEMBERS
	uint change_id;
	uint changed_index;
} ffui_tab;

FF_EXTERN int ffui_tab_create(ffui_tab *t, ffui_wnd *parent);

FF_EXTERN void ffui_tab_ins(ffui_tab *t, int idx, const char *textz);

#define ffui_tab_append(t, textz)  ffui_tab_ins(t, -1, textz)

#define ffui_tab_del(t, idx)  gtk_notebook_remove_page(GTK_NOTEBOOK((t)->h), idx)

#define ffui_tab_count(t)  gtk_notebook_get_n_pages(GTK_NOTEBOOK((t)->h))

#define ffui_tab_active(t)  gtk_notebook_get_current_page(GTK_NOTEBOOK((t)->h))

FF_EXTERN void ffui_tab_setactive(ffui_tab *t, int idx);


// STATUSBAR

FF_EXTERN int ffui_stbar_create(ffui_ctl *sb, ffui_wnd *parent);

static inline void ffui_stbar_settextz(ffui_ctl *sb, const char *text)
{
	gtk_statusbar_push(GTK_STATUSBAR(sb->h), gtk_statusbar_get_context_id(GTK_STATUSBAR(sb->h), "a"), text);
}


// TRAYICON
typedef struct ffui_trayicon {
	_FFUI_CTL_MEMBERS
	uint lclick_id;
} ffui_trayicon;

FF_EXTERN int ffui_tray_create(ffui_trayicon *t, ffui_wnd *wnd);

#define ffui_tray_hasicon(t) \
	(0 != gtk_status_icon_get_size(GTK_STATUS_ICON((t)->h)))

static inline void ffui_tray_seticon(ffui_trayicon *t, ffui_icon *ico)
{
	gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(t->h), ico->ico);
}

#define ffui_tray_show(t, show)  gtk_status_icon_set_visible(GTK_STATUS_ICON((t)->h), show)


// MESSAGE LOOP
FF_EXTERN void ffui_run();

#define ffui_quitloop()  gtk_main_quit()

typedef void (*ffui_handler)(void *param);

enum {
	FFUI_POST_WAIT = 1 << 31,
};

/**
flags: FFUI_POST_WAIT */
FF_EXTERN void ffui_thd_post(ffui_handler func, void *udata, uint flags);

enum FFUI_MSG {
	FFUI_QUITLOOP,
	FFUI_CHECKBOX_SETTEXTZ,
	FFUI_CLIP_SETTEXT,
	FFUI_EDIT_GETTEXT,
	FFUI_LBL_SETTEXT,
	FFUI_STBAR_SETTEXT,
	FFUI_TAB_ACTIVE,
	FFUI_TAB_COUNT,
	FFUI_TAB_INS,
	FFUI_TAB_SETACTIVE,
	FFUI_TEXT_ADDTEXT,
	FFUI_TEXT_SETTEXT,
	FFUI_TRK_SET,
	FFUI_TRK_SETRANGE,
	FFUI_VIEW_CLEAR,
	FFUI_VIEW_SCROLLSET,
	FFUI_VIEW_SETDATA,
	FFUI_WND_SETTEXT,
	FFUI_WND_SHOW,
};

/**
id: enum FFUI_MSG */
FF_EXTERN void ffui_post(void *ctl, uint id, void *udata);
FF_EXTERN ffsize ffui_send(void *ctl, uint id, void *udata);

#define ffui_post_quitloop()  ffui_post(NULL, FFUI_QUITLOOP, NULL)
#define ffui_send_lbl_settext(ctl, sz)  ffui_send(ctl, FFUI_LBL_SETTEXT, (void*)sz)
#define ffui_send_edit_textstr(ctl, str_dst)  ffui_send(ctl, FFUI_EDIT_GETTEXT, str_dst)
#define ffui_send_text_settextstr(ctl, str)  ffui_send(ctl, FFUI_TEXT_SETTEXT, (void*)str)
#define ffui_send_text_addtextstr(ctl, str)  ffui_send(ctl, FFUI_TEXT_ADDTEXT, (void*)str)
#define ffui_send_checkbox_settextz(ctl, sz)  ffui_send(ctl, FFUI_CHECKBOX_SETTEXTZ, (void*)sz)
#define ffui_send_wnd_settext(ctl, sz)  ffui_send(ctl, FFUI_WND_SETTEXT, (void*)sz)
#define ffui_post_wnd_show(ctl, show)  ffui_send(ctl, FFUI_WND_SHOW, (void*)(ffsize)show)
#define ffui_post_view_clear(ctl)  ffui_post(ctl, FFUI_VIEW_CLEAR, NULL)
#define ffui_post_view_scroll_set(ctl, vert_pos)  ffui_post(ctl, FFUI_VIEW_SCROLLSET, (void*)(ffsize)vert_pos)
#define ffui_clipboard_settextstr(str)  ffui_send(NULL, FFUI_CLIP_SETTEXT, (void*)str)

static inline void ffui_send_view_setdata(ffui_view *v, uint first, int delta)
{
	ffsize p = ((first & 0xffff) << 16) | (delta & 0xffff);
	ffui_send(v, FFUI_VIEW_SETDATA, (void*)p);
}
static inline void ffui_post_view_setdata(ffui_view *v, uint first, int delta)
{
	ffsize p = ((first & 0xffff) << 16) | (delta & 0xffff);
	ffui_post(v, FFUI_VIEW_SETDATA, (void*)p);
}
#define ffui_post_trk_setrange(ctl, range)  ffui_post(ctl, FFUI_TRK_SETRANGE, (void*)(ffsize)range)
#define ffui_post_trk_set(ctl, val)  ffui_post(ctl, FFUI_TRK_SET, (void*)(ffsize)val)

#define ffui_send_tab_ins(ctl, textz)  ffui_send(ctl, FFUI_TAB_INS, textz)
#define ffui_send_tab_setactive(ctl, idx)  ffui_send(ctl, FFUI_TAB_SETACTIVE, (void*)(ffsize)idx)
static inline int ffui_send_tab_active(ffui_tab *ctl)
{
	ffsize idx;
	ffui_send(ctl, FFUI_TAB_ACTIVE, &idx);
	return idx;
}
static inline int ffui_send_tab_count(ffui_tab *ctl)
{
	ffsize n;
	ffui_send(ctl, FFUI_TAB_COUNT, &n);
	return n;
}

#define ffui_send_stbar_settextz(sb, sz)  ffui_send(sb, FFUI_STBAR_SETTEXT, (void*)sz)


#ifdef __cplusplus
struct ffui_trackbarxx : ffui_trkbar {
	void set(uint value) { ffui_post_trk_set(this, value); }
	uint get() { return ffui_trk_val(this); }
	void range(uint range) { ffui_post_trk_setrange(this, range); }
};

struct ffui_stbarxx : ffui_ctl {
	void text(const char *sz) { ffui_send_stbar_settextz(this, sz); }
};

struct ffui_labelxx : ffui_label {
	void text(const char *sz) { ffui_send_lbl_settext(this, sz); }
	void markup(const char *sz) { gtk_label_set_markup(GTK_LABEL(h), sz); }
};

struct ffui_editxx : ffui_edit {
	ffstr text() { ffstr s = {}; ffui_edit_textstr(this, &s); return s; }
	void focus() { gtk_widget_grab_focus(h); }
};

struct ffui_tabxx : ffui_tab {
	void add(const char *sz) { ffui_tab_append(this, sz); }
};
#endif
