/** GUI/GTK+: loader
2019, Simon Zolin
*/

#include <FFOS/error.h>
#include "loader.h"
#include "dialog.h"
#include "view.h"
#include "window.h"
#include "../conf-scheme.h"
#include <FFOS/path.h>
#include <FFOS/file.h>

enum {
	FFUI_EINVAL = 100,
	FFUI_ENOMEM,
};

#define _F(x)  (ffsize)x
#define T_STR  FFCONF_TSTR
#define T_STRMULTI  (FFCONF_TSTR | FFCONF_FMULTI)
#define T_CLOSE  FFCONF_TCLOSE
#define T_OBJ  FFCONF_TOBJ
#define T_INT32  FFCONF_TINT32
#define T_STRLIST  (FFCONF_TSTR | FFCONF_FLIST)
#define T_OBJMULTI (FFCONF_TOBJ | FFCONF_FMULTI)

void* ffui_ldr_findctl(const ffui_ldr_ctl *ctx, void *ctl, const ffstr *name)
{
	ffstr s = *name, sctl;

	while (s.len != 0) {
		ffstr_splitby(&s, '.', &sctl, &s);
		for (uint i = 0; ; i++) {
			if (ctx[i].name == NULL)
				return NULL;

			if (ffstr_eqz(&sctl, ctx[i].name)) {
				uint off = ctx[i].flags & ~0x80000000;
				ctl = (char*)ctl + off;
				if (ctx[i].flags & 0x80000000)
					ctl = *(void**)ctl;
				if (s.len == 0)
					return ctl;

				if (ctx[i].children == NULL)
					return NULL;
				ctx = ctx[i].children;
				break;
			}
		}
	}
	return NULL;
}

static void* ldr_getctl(ffui_loader *g, const ffstr *name)
{
	char buf[255];
	ffstr s;
	s.ptr = buf;
	s.len = ffs_format_r0(buf, sizeof(buf), "%s.%S", g->wndname, name);
	return g->getctl(g->udata, &s);
}

static void ctl_place(ffui_ctl *ctl, ffui_loader *g)
{
	if (g->f_horiz) {
		if (g->hbox == NULL) {
			g->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_box_pack_start(GTK_BOX(g->wnd->vbox), g->hbox, /*expand=*/0, /*fill=*/0, /*padding=*/0);
		}
		gtk_box_pack_start(GTK_BOX(g->hbox), ctl->h, /*expand=*/0, /*fill=*/0, /*padding=*/0);
	} else {
		g->hbox = NULL;
		gtk_box_pack_start(GTK_BOX(g->wnd->vbox), ctl->h, /*expand=*/0, /*fill=*/0, /*padding=*/0);
	}
}

// ICON
// MENU ITEM
// MENU
// LABEL
// IMAGE
// BUTTON
// CHECKBOX
// EDITBOX
// TEXT
// TRACKBAR
// TAB
// VIEW COL
// VIEW
// STATUSBAR
// TRAYICON
// DIALOG
// WINDOW

// ICON
static const ffconf_arg icon_args[] = {
	{ "filename",	FFCONF_TSTRZ, FF_OFF(_ffui_ldr_icon, fn) },
	{}
};
static int icon_new(ffconf_scheme *cs, ffui_loader *g)
{
	ffmem_zero_obj(&g->ico_ctl);
	ffconf_scheme_addctx(cs, icon_args, &g->ico_ctl);
	return 0;
}
static int icon_done(ffui_loader *g, ffui_icon *icon)
{
	if (g->ico_ctl.fn == NULL) return 0;

	char *sz = ffsz_allocfmt("%S/%s", &g->path, g->ico_ctl.fn);
	ffui_icon_load(icon, sz);
	ffmem_free(sz);
	ffmem_free(g->ico_ctl.fn);
	ffmem_zero_obj(&g->ico_ctl);
	return 1;
}


// MENU ITEM
static int mi_submenu(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_menu *sub;

	if (NULL == (sub = g->getctl(g->udata, val)))
		return FFUI_EINVAL;

	ffui_menu_setsubmenu(g->mi, sub, g->wnd);
	return 0;
}

static int mi_action(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	int id;

	if (0 == (id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;

	ffui_menu_setcmd(g->mi, id);
	return 0;
}

static int mi_hotkey(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_wnd_hotkey *a;
	uint hk;

	if (NULL == (a = ffvec_pushT(&g->accels, ffui_wnd_hotkey)))
		return FFUI_ENOMEM;

	if (0 == (hk = ffui_hotkey_parse(val->ptr, val->len)))
		return FFUI_EINVAL;

	a->hk = hk;
	a->h = g->mi;
	return 0;
}

static int mi_done(ffconf_scheme *cs, ffui_loader *g)
{
	ffui_menu_ins(g->menu, g->mi, -1);
	return 0;
}

static const ffconf_arg mi_args[] = {
	{ "submenu",	T_STR,		_F(mi_submenu) },
	{ "action",		T_STR,		_F(mi_action) },
	{ "hotkey",		T_STR,		_F(mi_hotkey) },
	{ NULL,			T_CLOSE,	_F(mi_done) },
};

static int mi_new(ffconf_scheme *cs, ffui_loader *g)
{
	const ffstr *name = ffconf_scheme_objval(cs);

	if (ffstr_eqcz(name, "-"))
		g->mi = ffui_menu_newsep();
	else {
		ffstr s = vars_val(&g->vars, *name);
		char *sz = ffsz_dupstr(&s);
		g->mi = ffui_menu_new(sz);
		ffmem_free(sz);
	}

	ffconf_scheme_addctx(cs, mi_args, g);
	return 0;
}


// MENU
static const ffconf_arg menu_args[] = {
	{ "item",	T_OBJMULTI,	_F(mi_new) },
	{}
};

static int menu_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->menu = g->getctl(g->udata, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_menu_create(g->menu))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, menu_args, g);
	return 0;
}

static int mmenu_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->menu = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_menu_createmain(g->menu))
		return FFUI_ENOMEM;

	ffui_wnd_setmenu(g->wnd, g->menu);
	ffconf_scheme_addctx(cs, menu_args, g);
	return 0;
}


// LABEL
static int lbl_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "horizontal"))
		g->f_horiz = 1;
	else
		return FFUI_EINVAL;
	return 0;
}
static int lbl_text(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_lbl_settextstr(g->lbl, val);
	return 0;
}
static int lbl_done(ffconf_scheme *cs, ffui_loader *g)
{
	ctl_place(g->ctl, g);
	return 0;
}
static const ffconf_arg lbl_args[] = {
	{ "style",	T_STRLIST,	_F(lbl_style) },
	{ "text",	T_STR,		_F(lbl_text) },
	{ NULL,		T_CLOSE,	_F(lbl_done) },
};

static int lbl_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_lbl_create(g->lbl, g->wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, lbl_args, g);
	g->flags = 0;
	return 0;
}


// IMAGE
static int img_done(ffconf_scheme *cs, ffui_loader *g)
{
	ctl_place(g->ctl, g);

	ffui_icon ico;
	if (icon_done(g, &ico))
		ffui_image_seticon(g->img, &ico);
	return 0;
}
static const ffconf_arg img_args[] = {
	{ "icon",	T_OBJ,		_F(icon_new) },
	{ NULL,		T_CLOSE,	_F(img_done) },
};
static int img_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_image_create(g->img, g->wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, img_args, g);
	g->flags = 0;
	return 0;
}


// BUTTON
static int btn_text(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffstr s = vars_val(&g->vars, *val);
	ffui_btn_settextstr(g->btn, &s);
	return 0;
}
static int btn_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "horizontal"))
		g->f_horiz = 1;
	else
		return FFUI_EINVAL;
	return 0;
}
static int btn_action(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->btn->action_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static int btn_done(ffconf_scheme *cs, ffui_loader *g)
{
	ctl_place(g->ctl, g);

	ffui_icon ico;
	if (icon_done(g, &ico))
		ffui_btn_seticon(g->btn, &ico);
	return 0;
}
static const ffconf_arg btn_args[] = {
	{ "style",	T_STRLIST,	_F(btn_style) },
	{ "action",	T_STR,		_F(btn_action) },
	{ "text",	T_STR,		_F(btn_text) },
	{ "icon",	T_OBJ,		_F(icon_new) },
	{ NULL,		T_CLOSE,	_F(btn_done) },
};

static int btn_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_btn_create(g->btn, g->wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, btn_args, g);
	g->flags = 0;
	return 0;
}

// CHECKBOX
static int chbox_text(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_checkbox_settextstr(g->cb, val);
	return 0;
}
static int chbox_action(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->cb->action_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg chbox_args[] = {
	{ "style",	T_STRLIST,	_F(btn_style) },
	{ "action",	T_STR,		_F(chbox_action) },
	{ "text",	T_STR,		_F(chbox_text) },
	{ NULL,		T_CLOSE,	_F(btn_done) },
};

static int chbox_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_checkbox_create(g->cb, g->wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, chbox_args, g);
	g->flags = 0;
	return 0;
}


// EDITBOX
static int edit_done(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->f_horiz) {
		if (g->hbox == NULL) {
			g->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_box_pack_start(GTK_BOX(g->wnd->vbox), g->hbox, /*expand=*/0, /*fill=*/0, /*padding=*/0);
		}
		gtk_box_pack_start(GTK_BOX(g->hbox), g->edit->h, /*expand=*/1, /*fill=*/1, /*padding=*/0);
	} else {
		g->hbox = NULL;
		gtk_box_pack_start(GTK_BOX(g->wnd->vbox), g->edit->h, /*expand=*/0, /*fill=*/0, /*padding=*/0);
	}

	return 0;
}
static int edit_text(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_edit_settextstr(g->edit, val);
	return 0;
}
static int edit_onchange(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->edit->change_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg edit_args[] = {
	{ "style",	T_STRLIST,	_F(btn_style) },
	{ "text",	T_STR,		_F(edit_text) },
	{ "onchange",T_STR,		_F(edit_onchange) },
	{ NULL,		T_CLOSE,	_F(edit_done) },
};

static int edit_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_edit_create(g->edit, g->wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, edit_args, g);
	g->flags = 0;
	return 0;
}


// TEXT
static int text_done(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->f_horiz) {
		if (g->hbox == NULL) {
			g->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_box_pack_start(GTK_BOX(g->wnd->vbox), g->hbox, /*expand=*/1, /*fill=*/0, /*padding=*/0);
		}
		gtk_box_pack_start(GTK_BOX(g->hbox), g->text->h, /*expand=*/1, /*fill=*/1, /*padding=*/0);
	} else {
		void *scrl = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(scrl), g->text->h);
		gtk_scrolled_window_set_policy(scrl, GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
		g->hbox = NULL;
		gtk_box_pack_start(GTK_BOX(g->wnd->vbox), scrl, /*expand=*/1, /*fill=*/1, /*padding=*/0);
	}

	return 0;
}
static const ffconf_arg text_args[] = {
	{ "style",	T_STRLIST,	_F(btn_style) },
	{ NULL,		T_CLOSE,	_F(text_done) },
};

static int text_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_text_create(g->text, g->wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, text_args, g);
	g->flags = 0;
	return 0;
}


// TRACKBAR
static int trkbar_range(ffconf_scheme *cs, ffui_loader *g, ffint64 val)
{
	ffui_trk_setrange(g->trkbar, val);
	return 0;
}
static int trkbar_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "horizontal"))
		g->f_horiz = 1;
	else
		return FFUI_EINVAL;
	return 0;
}
static int trkbar_val(ffconf_scheme *cs, ffui_loader *g, ffint64 val)
{
	ffui_trk_set(g->trkbar, val);
	return 0;
}
static int trkbar_onscroll(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->trkbar->scroll_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static int trkbar_done(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->f_loadconf)
		return 0;
	if (g->f_horiz) {
		if (g->hbox == NULL) {
			g->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_box_pack_start(GTK_BOX(g->wnd->vbox), g->hbox, /*expand=*/0, /*fill=*/0, /*padding=*/0);
		}
		gtk_box_pack_start(GTK_BOX(g->hbox), g->trkbar->h, /*expand=*/1, /*fill=*/1, /*padding=*/0);
	} else {
		g->hbox = NULL;
		gtk_box_pack_start(GTK_BOX(g->wnd->vbox), g->trkbar->h, /*expand=*/0, /*fill=*/0, /*padding=*/0);
	}
	return 0;
}
static const ffconf_arg trkbar_args[] = {
	{ "style",		T_STRLIST,	_F(trkbar_style) },
	{ "range",		T_INT32,	_F(trkbar_range) },
	{ "onscroll",	T_STR,		_F(trkbar_onscroll) },
	{ "value",		T_INT32,	_F(trkbar_val) },
	{ NULL,			T_CLOSE,	_F(trkbar_done) },
};

static int trkbar_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->trkbar = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_trk_create(g->trkbar, g->wnd))
		return FFUI_ENOMEM;
	ffconf_scheme_addctx(cs, trkbar_args, g);
	g->flags = 0;
	return 0;
}


// TAB
static int tab_onchange(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->tab->change_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg tab_args[] = {
	{ "onchange",	T_STR,	_F(tab_onchange) },
	{}
};
static int tab_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->tab = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_tab_create(g->tab, g->wnd))
		return FFUI_ENOMEM;
	ffconf_scheme_addctx(cs, tab_args, g);
	return 0;
}


// VIEW COL
static int viewcol_width(ffconf_scheme *cs, ffui_loader *g, ffint64 val)
{
	ffui_viewcol_setwidth(&g->vicol, val);
	return 0;
}

static int viewcol_done(ffconf_scheme *cs, ffui_loader *g)
{
	ffui_view_inscol(g->vi, -1, &g->vicol);
	return 0;
}

static const ffconf_arg viewcol_args[] = {
	{ "width",	T_INT32,	_F(viewcol_width) },
	{ NULL,		T_CLOSE,	_F(viewcol_done) },
};

static int viewcol_new(ffconf_scheme *cs, ffui_loader *g)
{
	ffstr *name = ffconf_scheme_objval(cs);
	ffui_viewcol_reset(&g->vicol);
	ffui_viewcol_setwidth(&g->vicol, 100);
	ffstr s = vars_val(&g->vars, *name);
	ffui_viewcol_settext(&g->vicol, s.ptr, s.len);
	ffconf_scheme_addctx(cs, viewcol_args, g);
	return 0;
}

// VIEW

enum {
	VIEW_STYLE_EDITABLE,
	VIEW_STYLE_GRID_LINES,
	VIEW_STYLE_MULTI_SELECT,
};
static const char *const view_styles_sorted[] = {
	"editable",
	"grid_lines",
	"multi_select",
};
static int view_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	int n = ffszarr_ifindsorted(view_styles_sorted, FF_COUNT(view_styles_sorted), val->ptr, val->len);
	switch (n) {

	case VIEW_STYLE_GRID_LINES:
		ffui_view_style(g->vi, FFUI_VIEW_GRIDLINES, FFUI_VIEW_GRIDLINES);
		break;

	case VIEW_STYLE_MULTI_SELECT:
		ffui_view_style(g->vi, FFUI_VIEW_MULTI_SELECT, FFUI_VIEW_MULTI_SELECT);
		break;

	case VIEW_STYLE_EDITABLE:
		ffui_view_style(g->vi, FFUI_VIEW_EDITABLE, FFUI_VIEW_EDITABLE);
		break;

	default:
		return FFUI_EINVAL;
	}
	return 0;
}

static int view_dblclick(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->vi->dblclick_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}

static const ffconf_arg view_args[] = {
	{ "style",		T_STRLIST,	_F(view_style) },
	{ "dblclick",	T_STR,		_F(view_dblclick) },
	{ "column",		T_OBJMULTI,	_F(viewcol_new) },
	{}
};

static int view_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_view_create(g->vi, g->wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, view_args, g);
	return 0;
}


// STATUSBAR
static const ffconf_arg stbar_args[] = {
	{}
};
static int stbar_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	if (0 != ffui_stbar_create(g->ctl, g->wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, stbar_args, g);
	return 0;
}


// TRAYICON
static int tray_lclick(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->trayicon->lclick_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg tray_args[] = {
	{ "lclick",	T_STR,	_F(tray_lclick) },
	{}
};
static int tray_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->trayicon = ldr_getctl(g, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;

	ffui_tray_create(g->trayicon, g->wnd);
	ffconf_scheme_addctx(cs, tray_args, g);
	return 0;
}


// DIALOG
static const ffconf_arg dlg_args[] = {
	{}
};
static int dlg_new(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->dlg = g->getctl(g->udata, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;
	ffui_dlg_init(g->dlg);
	ffconf_scheme_addctx(cs, dlg_args, g);
	return 0;
}


// WINDOW
static int wnd_title(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffstr s = vars_val(&g->vars, *val);
	ffui_wnd_settextstr(g->wnd, &s);
	return 0;
}

static int wnd_popupfor(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	void *parent;
	if (NULL == (parent = g->getctl(g->udata, val)))
		return FFUI_EINVAL;
	ffui_wnd_setpopup(g->wnd, parent);
	return 0;
}

static int wnd_position(ffconf_scheme *cs, ffui_loader *g, ffint64 v)
{
	int *i = &g->r.x;
	if (g->list_idx == 4)
		return FFUI_EINVAL;
	i[g->list_idx] = (int)v;
	if (g->list_idx == 3) {
		ffui_wnd_setplacement(g->wnd, 0, &g->r);
	}
	g->list_idx++;
	return 0;
}

static int wnd_icon(ffconf_scheme *cs, ffui_loader *g)
{
	ffmem_zero_obj(&g->ico);
	ffconf_scheme_addctx(cs, icon_args, &g->ico);
	return 0;
}

static int wnd_done(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->ico.fn != NULL) {
		ffui_icon ico;
		char *sz = ffsz_allocfmt("%S/%s", &g->path, g->ico.fn);
		ffui_icon_load(&ico, sz);
		ffmem_free(sz);
		ffui_wnd_seticon(g->wnd, &ico);
		// ffmem_free(g->ico.fn);
		// ffmem_zero_obj(&g->ico);
	}

	if (g->accels.len != 0) {
		int r = ffui_wnd_hotkeys(g->wnd, (ffui_wnd_hotkey*)g->accels.ptr, g->accels.len);
		g->accels.len = 0;
		if (r != 0)
			return FFUI_ENOMEM;
	}

	return 0;
}

static const ffconf_arg wnd_args[] = {
	{ "title",		T_STR,		_F(wnd_title) },
	{ "position",	FFCONF_TINT32 | FFCONF_FSIGN | FFCONF_FLIST, _F(wnd_position) },
	{ "icon",		T_OBJ,		_F(wnd_icon) },
	{ "popupfor",	T_STR,		_F(wnd_popupfor) },

	{ "mainmenu",	T_OBJ,		_F(mmenu_new) },
	{ "button",		T_OBJMULTI,	_F(btn_new) },
	{ "checkbox",	T_OBJMULTI,	_F(chbox_new) },
	{ "label",		T_OBJMULTI,	_F(lbl_new) },
	{ "image",		T_OBJMULTI,	_F(img_new) },
	{ "editbox",	T_OBJMULTI,	_F(edit_new) },
	{ "text",		T_OBJMULTI,	_F(text_new) },
	{ "trackbar",	T_OBJMULTI,	_F(trkbar_new) },
	{ "tab",		T_OBJMULTI,	_F(tab_new) },
	{ "listview",	T_OBJMULTI,	_F(view_new) },
	{ "statusbar",	T_OBJ,		_F(stbar_new) },
	{ "trayicon",	T_OBJ,		_F(tray_new) },
	{ NULL,			T_CLOSE,	_F(wnd_done) },
};

static int wnd_new(ffconf_scheme *cs, ffui_loader *g)
{
	ffui_wnd *wnd;

	if (NULL == (wnd = g->getctl(g->udata, ffconf_scheme_objval(cs))))
		return FFUI_EINVAL;
	ffmem_zero((ffbyte*)g + FF_OFF(ffui_loader, wnd), sizeof(ffui_loader) - FF_OFF(ffui_loader, wnd));
	g->wnd = wnd;
	if (NULL == (g->wndname = ffsz_dupn(cs->objval.ptr, cs->objval.len)))
		return FFUI_ENOMEM;
	g->ctl = (ffui_ctl*)wnd;
	if (0 != ffui_wnd_create(wnd))
		return FFUI_ENOMEM;

	ffconf_scheme_addctx(cs, wnd_args, g);
	return 0;
}


// LANGUAGE
static int inc_lang_entry(ffconf_scheme *cs, ffui_loader *g, ffstr *fn)
{
	const ffstr *lang = ffconf_scheme_keyname(cs);
	if (!(ffstr_eqz(lang, "default")
		|| ffstr_eq(lang, g->language, 2)))
		return 0;
	if (g->lang_data.len != 0)
		return FFUI_EINVAL;

	if (ffstr_findanyz(fn, "/\\") >= 0)
		return FFUI_EINVAL;

	int rc = FFUI_EINVAL;
	char *ffn = ffsz_allocfmt("%S/%S", &g->path, fn);
	ffvec d = {};
	if (0 != fffile_readwhole(ffn, &d, 64*1024))
		goto end;

	rc = vars_load(&g->vars, *(ffstr*)&d);

end:
	if (rc == 0) {
		if (ffstr_eqz(lang, "default"))
			g->lang_data_def = d;
		else
			g->lang_data = d;
		ffvec_null(&d);
	}
	ffvec_free(&d);
	ffmem_free(ffn);
	return rc;
}
static const ffconf_arg inc_lang_args[] = {
	{ "*", T_STRMULTI, (ffsize)inc_lang_entry },
	{}
};
static int inc_lang(ffconf_scheme *cs, ffui_loader *g)
{
	ffconf_scheme_addctx(cs, inc_lang_args, g);
	return 0;
}


static const ffconf_arg top_args[] = {
	{ "window",	T_OBJMULTI,	_F(wnd_new) },
	{ "menu",	T_OBJMULTI,	_F(menu_new) },
	{ "dialog",	T_OBJMULTI,	_F(dlg_new) },
	{ "include_language",	T_OBJ,	_F(inc_lang) },
	{}
};

void ffui_ldr_init(ffui_loader *g, ffui_ldr_getctl_t getctl, ffui_ldr_getcmd_t getcmd, void *udata)
{
	ffmem_zero_obj(g);
	g->getctl = getctl;
	g->getcmd = getcmd;
	g->udata = udata;
	vars_init(&g->vars);
}

void ffui_ldr_fin(ffui_loader *g)
{
	ffvec_free(&g->lang_data_def);
	ffvec_free(&g->lang_data);
	ffmem_free(g->ico_ctl.fn);
	ffmem_free(g->ico.fn);
	ffvec_free(&g->accels);
	ffmem_free(g->errstr);
	ffmem_free(g->wndname);
	vars_free(&g->vars);
}

int ffui_ldr_loadfile(ffui_loader *g, const char *fn)
{
	ffpath_splitpath(fn, ffsz_len(fn), &g->path, NULL);
	g->path.len += FFS_LEN("/");

	ffstr errstr = {};
	int r = ffconf_parse_file(top_args, g, fn, 0, &errstr, 1*1024*1024);
	if (r != 0) {
		ffmem_free(g->errstr);
		g->errstr = ffsz_dupstr(&errstr);
	}

	ffstr_free(&errstr);
	return r;
}
