/** GUI-winapi loader
2014, Simon Zolin */

#include "loader.h"
#include <FFOS/path.h>
#include <FFOS/file.h>


static void* ldr_getctl(ffui_loader *g, const ffstr *name);

enum {
	FFUI_EINVAL = 100,
	FFUI_ENOMEM,
	FFUI_ESYS,
};

#define _F(f)  (ffsize)(f)
#define T_CLOSE  FFCONF_TCLOSE
#define T_OBJ  FFCONF_TOBJ
#define T_OBJMULTI  (FFCONF_TOBJ | FFCONF_FMULTI)
#define T_INT32  FFCONF_TINT32
#define T_INTLIST  (FFCONF_TINT32 | FFCONF_FLIST)
#define T_INTLIST_S  (FFCONF_TINT32 | FFCONF_FSIGN | FFCONF_FLIST)
#define T_STR  FFCONF_TSTR
#define T_STRMULTI  (FFCONF_TSTR | FFCONF_FMULTI)
#define T_STRLIST  (FFCONF_TSTR | FFCONF_FLIST)
#define FF_XSPACE  2
#define FF_YSPACE  2

static void state_reset(ffui_loader *g)
{
	g->list_idx = 0;
	g->style_reset = 0;
}
static void state_reset2(ffui_loader *g)
{
	g->r.x = 0;
	g->r.y = 0;
	g->r.cx = 200;
	g->r.cy = 20;
	g->style_horizontal = 0;
	g->auto_pos = 1;
	g->man_pos = 0;
	state_reset(g);
}

static void ctl_setpos(ffui_loader *g)
{
	ffui_pos *p = &g->prev_ctl_pos;
	if (g->auto_pos) {
		g->auto_pos = 0;
		g->man_pos = 1;

		if (g->style_horizontal) {
			g->style_horizontal = 0;
			g->r.x = p->x + p->cx + FF_XSPACE;
			g->r.y = p->y;

			p->x = g->r.x;
			p->cx = g->r.cx;
			p->y = g->r.y;
			p->cy = ffmax(p->cy, g->r.cy);

		} else {
			g->r.x = 0;
			g->r.y = p->y + p->cy + FF_YSPACE;

			p->x = 0;
			p->cx = g->r.cx;
			p->y = g->r.y;
			p->cy = g->r.cy;
		}

	} else {
		p->x = g->r.x;
		p->cx = g->r.cx;
		p->y = g->r.y;
		p->cy = ffmax(p->cy, g->r.cy);
	}

	if (g->man_pos) {
		g->man_pos = 0;
		ffui_setposrect(g->ctl, &g->r, 0);
	}
}
static int ctl_pos(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	if (g->list_idx == 4)
		return FFUI_EINVAL;
	int *i = &g->r.x;
	i[g->list_idx] = (int)val;
	g->man_pos = 1;
	g->auto_pos = 0;
	g->list_idx++;
	return 0;
}
static int ctl_size(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	if (g->list_idx == 2)
		return FFUI_EINVAL;
	if (g->list_idx == 0)
		g->r.cx = (int)val;
	else
		g->r.cy = (int)val;
	g->auto_pos = 1;
	g->list_idx++;
	return 0;
}
static int ctl_resize(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "cx"))
		g->resize_flags |= 1;
	else if (ffstr_eqcz(val, "cy"))
		g->resize_flags |= 2;
	return 0;
}
static void ctl_autoresize(ffui_loader *g)
{
	if (g->resize_flags == 0)
		return;
	ffui_paned *pn = ffmem_new(ffui_paned);
	*ffvec_pushT(&g->paned_array, ffui_paned*) = pn;
	pn->items[0].it = g->ctl;
	pn->items[0].cx = !!(g->resize_flags & 1);
	pn->items[0].cy = !!(g->resize_flags & 2);
	ffui_paned_create(pn, g->wnd);
	g->resize_flags = 0;
}
static int ctl_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "visible"))
		;
	else if (ffstr_eqcz(val, "horizontal"))
		g->style_horizontal = 1;
	else
		return FFUI_EINVAL;

	return 0;
}
static int ctl_done(ffconf_scheme *cs, ffui_loader *g)
{
	ctl_setpos(g);
	ctl_autoresize(g);
	ffui_show(g->ctl, 1);
	return 0;
}

static int ico_size(ffconf_scheme *cs,	_ffui_ldr_icon_t *ico, int64 val)
{
	switch (ico->ldr->list_idx) {
	case 0:
		ico->cx = val; break;
	case 1:
		ico->cy = val; break;
	default:
		return FFUI_EINVAL;
	}
	ico->ldr->list_idx++;
	return 0;
}
static int ico_done(ffconf_scheme *cs,	_ffui_ldr_icon_t *ico)
{
	char *p, fn[4096];

	if (ico->resource != 0) {
		wchar_t wname[256];
		ffs_format(fn, sizeof(fn), "#%u%Z", ico->resource);
		size_t wname_len = FF_COUNT(wname);
		ffs_utow(wname, &wname_len, fn, -1);
		if (0 != ffui_icon_loadres(&ico->icon, wname, ico->cx, ico->cy)) {
			if (ico->cx == 0)
				return FFUI_ESYS;
			if (0 != ffui_icon_loadres(&ico->icon, wname, 0, 0))
				return FFUI_ESYS;
		}
		if (ico->load_small
			&& 0 != ffui_icon_loadres(&ico->icon_small, wname, 16, 16)) {

			if (0 != ffui_icon_loadres(&ico->icon_small, wname, 0, 0))
				return FFUI_ESYS;
		}
		return 0;
	}

	p = fn + ffmem_ncopy(fn, FF_COUNT(fn), ico->ldr->path.ptr, ico->ldr->path.len);
	ffsz_copyn(p, fn + FF_COUNT(fn) - p, ico->fn.ptr, ico->fn.len);
	ffstr_free(&ico->fn);
	if (ico->cx != 0) {
		if (0 != ffui_icon_loadimg(&ico->icon, fn, ico->cx, ico->cy, FFUI_ICON_DPISCALE)) {
			//Note: winXP can't read PNG-compressed icons.  Load the first icon.
			if (0 != ffui_icon_load(&ico->icon, fn, 0, 0))
				return FFUI_ESYS;
		}
	} else {
		if (0 != ffui_icon_load(&ico->icon, fn, 0, 0))
			return FFUI_ESYS;
		if (ico->load_small && 0 != ffui_icon_load(&ico->icon_small, fn, 0, FFUI_ICON_SMALL))
			return FFUI_ESYS;
	}
	return 0;
}
static const ffconf_arg icon_args[] = {
	{ "filename",	T_STR,		FF_OFF(_ffui_ldr_icon_t, fn) },
	{ "resource",	T_INT32,	FF_OFF(_ffui_ldr_icon_t, resource) },
	{ "index",		T_INT32,	FF_OFF(_ffui_ldr_icon_t, idx) },
	{ "size",		T_INTLIST,	_F(ico_size) },
	{ NULL,			T_CLOSE,	_F(ico_done) },
};


static int mi_submenu(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_menu *sub;

	if (NULL == (sub = g->getctl(g->udata, val)))
		return FFUI_EINVAL;

	ffui_menu_setsubmenu(&g->menuitem.mi, sub->h);
	return 0;
}

static int mi_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "checked"))
		ffui_menu_addstate(&g->menuitem.mi, FFUI_MENU_CHECKED);

	else if (ffstr_eqcz(val, "default"))
		ffui_menu_addstate(&g->menuitem.mi, FFUI_MENU_DEFAULT);

	else if (ffstr_eqcz(val, "disabled"))
		ffui_menu_addstate(&g->menuitem.mi, FFUI_MENU_DISABLED);

	else if (ffstr_eqcz(val, "radio"))
		ffui_menu_settype(&g->menuitem.mi, FFUI_MENU_RADIOCHECK);

	else
		return FFUI_EINVAL;

	return 0;
}

static int mi_action(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	int id;
	if (0 == (id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	ffui_menu_setcmd(&g->menuitem.mi, id);
	return 0;
}

/** Append hotkey to menu item text */
static void ffui_menu_sethotkey(ffui_menuitem *mi, const char *s, size_t len)
{
	wchar_t *w = mi->dwTypeData;
	size_t textlen = wcslen(w);
	size_t cap = textlen + FFS_LEN("\t") + len + 1;
	if (mi->dwTypeData == NULL)
		return;

	if (NULL == (w = ffmem_realloc(w, cap * sizeof(wchar_t))))
		return;
	mi->dwTypeData = w;
	w += textlen;
	*w++ = '\t';
	ffsz_utow_n(w, cap, s, len);
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

	ffui_menu_sethotkey(&g->menuitem.mi, val->ptr, val->len);
	g->menuitem.iaccel = 1;
	return 0;
}

static int mi_done(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->menuitem.iaccel && g->menuitem.mi.wID != 0) {
		ffui_wnd_hotkey *hk = ffslice_lastT(&g->accels, ffui_wnd_hotkey);
		hk->cmd = g->menuitem.mi.wID;
	}

	if (0 != ffui_menu_append(g->menu, &g->menuitem.mi))
		return FFUI_ENOMEM;
	return 0;
}
static const ffconf_arg menuitem_args[] = {
	{ "submenu",	T_STR,		_F(mi_submenu) },
	{ "style",		T_STRLIST,	_F(mi_style) },
	{ "action",		T_STR,		_F(mi_action) },
	{ "hotkey",		T_STR,		_F(mi_hotkey) },
	{ NULL,			T_CLOSE,	_F(mi_done) },
};
static int new_menuitem(ffconf_scheme *cs, ffui_loader *g)
{
	ffmem_zero_obj(&g->menuitem);

	if (ffstr_eqcz(&cs->objval, "-"))
		ffui_menu_settype(&g->menuitem.mi, FFUI_MENU_SEPARATOR);
	else {
		ffstr s = vars_val(&g->vars, cs->objval);
		ffui_menu_settextstr(&g->menuitem.mi, &s);
	}

	state_reset(g);
	ffconf_scheme_addctx(cs, menuitem_args, g);
	return 0;
}

static const ffconf_arg menu_args[] = {
	{ "item",	T_OBJMULTI,	_F(new_menuitem) },
	{}
};
static int new_menu(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->menu = g->getctl(g->udata, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_menu_create(g->menu))
		return FFUI_ENOMEM;
	state_reset(g);
	ffconf_scheme_addctx(cs, menu_args, g);
	return 0;
}

static int new_mmenu(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->menu = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	ffui_menu_createmain(g->menu);

	if (!SetMenu(g->wnd->h, g->menu->h))
		return FFUI_ENOMEM;

	state_reset(g);
	ffconf_scheme_addctx(cs, menu_args, g);
	return 0;
}


static int trkbar_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "no_ticks"))
		ffui_styleset(g->trkbar->h, TBS_NOTICKS);
	else if (ffstr_eqcz(val, "both"))
		ffui_styleset(g->trkbar->h, TBS_BOTH);
	else if (ffstr_eqcz(val, "horizontal"))
		g->style_horizontal = 1;
	return 0;
}
static int trkbar_range(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	ffui_trk_setrange(g->trkbar, val);
	return 0;
}
static int trkbar_val(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	ffui_trk_set(g->trkbar, val);
	return 0;
}
static int trkbar_pagesize(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	ffui_trk_setpage(g->trkbar, val);
	return 0;
}
static int trkbar_onscroll(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->trkbar->scroll_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static int trkbar_onscrolling(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->trkbar->scrolling_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg trkbar_args[] = {
	{ "style",		T_STRLIST,	_F(trkbar_style) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ "range",		T_INT32,	_F(trkbar_range) },
	{ "value",		T_INT32,	_F(trkbar_val) },
	{ "page_size",	T_INT32,	_F(trkbar_pagesize) },
	{ "onscroll",	T_STR,		_F(trkbar_onscroll) },
	{ "onscrolling",T_STR,		_F(trkbar_onscrolling) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};
static int new_trkbar(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->trkbar = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_trk_create(g->trkbar, g->wnd))
		return FFUI_ENOMEM;
	state_reset2(g);
	ffconf_scheme_addctx(cs, trkbar_args, g);
	return 0;
}


static const ffconf_arg pgsbar_args[] = {
	{ "style",		T_STRLIST,	_F(ctl_style) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};
static int new_pgsbar(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_pgs_create(g->ctl, g->wnd))
		return FFUI_ENOMEM;
	state_reset(g);
	ffconf_scheme_addctx(cs, pgsbar_args, g);
	return 0;
}


static int stbar_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	return 0;
}
static int stbar_parts(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	int *it = ffvec_pushT(&g->sb_parts, int);
	if (it == NULL)
		return FFUI_ENOMEM;
	*it = (int)val;
	return 0;
}
static int stbar_done(ffconf_scheme *cs, ffui_loader *g)
{
	ffui_stbar_setparts(g->ctl, g->sb_parts.len, g->sb_parts.ptr);
	ffvec_free(&g->sb_parts);
	ffui_show(g->ctl, 1);
	return 0;
}
static const ffconf_arg stbar_args[] = {
	{ "style",		T_STRLIST,	_F(stbar_style) },
	{ "parts",		T_INTLIST_S,_F(stbar_parts) },
	{ NULL,			T_CLOSE,	_F(stbar_done) },
};
static int new_stbar(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_stbar_create(g->actl.stbar, g->wnd))
		return FFUI_ENOMEM;
	ffvec_null(&g->sb_parts);
	state_reset(g);
	ffconf_scheme_addctx(cs, stbar_args, g);
	return 0;
}


static int tray_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "visible"))
		g->tr.show = 1;
	else
		return FFUI_EINVAL;
	return 0;
}
static int tray_pmenu(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_menu *m = g->getctl(g->udata, val);
	if (m == NULL)
		return FFUI_EINVAL;
	g->wnd->trayicon->pmenu = m;
	return 0;
}
static int tray_icon(ffconf_scheme *cs, ffui_loader *g)
{
	ffmem_zero_obj(&g->ico_ctl);
	g->ico_ctl.ldr = g;
	g->ico_ctl.cx = g->ico_ctl.cy = 16;
	state_reset(g);
	ffconf_scheme_addctx(cs, icon_args, &g->ico_ctl);
	return 0;
}
static int tray_lclick(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->wnd->trayicon->lclick_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static int tray_done(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->ico_ctl.icon.h != NULL)
		ffui_tray_seticon(g->tray, &g->ico_ctl.icon);

	if (g->tr.show && 0 != ffui_tray_show(g->tray, 1))
		return FFUI_ENOMEM;

	return 0;
}
static const ffconf_arg tray_args[] = {
	{ "style",		T_STRLIST,	_F(tray_style) },
	{ "icon",		T_OBJ,		_F(tray_icon) },
	{ "popupmenu",	T_STR,		_F(tray_pmenu) },
	{ "lclick",		T_STR,		_F(tray_lclick) },
	{ NULL,			T_CLOSE,	_F(tray_done) },
};
static int new_tray(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->tray = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	ffui_tray_create(g->tray, g->wnd);
	ffmem_zero_obj(&g->ico_ctl);
	state_reset(g);
	ffconf_scheme_addctx(cs, tray_args, g);
	return 0;
}


static int font_name(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_font_set(&g->fnt, val, 0, 0);
	return 0;
}

static int font_height(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	ffui_font_set(&g->fnt, NULL, (int)val, 0);
	return 0;
}

static int font_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	uint f = 0;
	if (ffstr_eqcz(val, "bold"))
		f = FFUI_FONT_BOLD;
	else if (ffstr_eqcz(val, "italic"))
		f = FFUI_FONT_ITALIC;
	else if (ffstr_eqcz(val, "underline"))
		f = FFUI_FONT_UNDERLINE;
	else
		return FFUI_EINVAL;
	ffui_font_set(&g->fnt, NULL, 0, f);
	return 0;
}

static int font_done(ffconf_scheme *cs, ffui_loader *g)
{
	HFONT f;
	f = ffui_font_create(&g->fnt);
	if (f == NULL)
		return FFUI_ENOMEM;
	if (g->ctl == (void*)g->wnd)
		g->wnd->font = f;
	else {
		ffui_ctl_send(g->ctl, WM_SETFONT, f, 0);
		g->ctl->font = f;
	}
	return 0;
}
static const ffconf_arg font_args[] = {
	{ "name",		T_STR,		_F(font_name) },
	{ "height",		T_INT32,	_F(font_height) },
	{ "style",		T_STRLIST,	_F(font_style) },
	{ NULL,			T_CLOSE,	_F(font_done) },
};


static int label_text(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffstr s = vars_val(&g->vars, *val);
	ffui_settextstr(g->ctl, &s);
	return 0;
}

static int label_font(ffconf_scheme *cs, ffui_loader *g)
{
	ffmem_zero(&g->fnt, sizeof(LOGFONT));
	g->fnt.lf.lfHeight = 15;
	g->fnt.lf.lfWeight = FW_NORMAL;
	state_reset(g);
	ffconf_scheme_addctx(cs, font_args, g);
	return 0;
}

static const char *const _ffpic_clrstr[] = {
	"aqua",
	"black",
	"blue",
	"fuchsia",
	"green",
	"grey",
	"lime",
	"maroon",
	"navy",
	"olive",
	"orange",
	"purple",
	"red",
	"silver",
	"teal",
	"white",
	"yellow",
};
static const uint ffpic_clr_a[] = {
	/*aqua*/	0x7fdbff,
	/*black*/	0x111111,
	/*blue*/	0x0074d9,
	/*fuchsia*/	0xf012be,
	/*green*/	0x2ecc40,
	/*grey*/	0xaaaaaa,
	/*lime*/	0x01ff70,
	/*maroon*/	0x85144b,
	/*navy*/	0x001f3f,
	/*olive*/	0x3d9970,
	/*orange*/	0xff851b,
	/*purple*/	0xb10dc9,
	/*red*/	0xff4136,
	/*silver*/	0xdddddd,
	/*teal*/	0x39cccc,
	/*white*/	0xffffff,
	/*yellow*/	0xffdc00,
};

uint ffpic_color3(const char *s, ffsize len, const uint *clrs)
{
	ffssize n;
	uint clr = (uint)-1;

	if (len == FFS_LEN("#rrggbb") && s[0] == '#') {
		if (FFS_LEN("rrggbb") != ffs_toint(s + 1, len - 1, &clr, FFS_INT32 | FFS_INTHEX))
			goto err;

	} else {
		if (-1 == (n = ffszarr_ifindsorted(_ffpic_clrstr, FF_COUNT(_ffpic_clrstr), s, len)))
			goto err;
		clr = clrs[n];
	}

	//LE: BGR0 -> RGB0
	//BE: 0RGB -> RGB0
	union {
		uint i;
		byte b[4];
	} u;
	u.b[0] = ((clr & 0xff0000) >> 16);
	u.b[1] = ((clr & 0x00ff00) >> 8);
	u.b[2] = (clr & 0x0000ff);
	u.b[3] = 0;
	clr = u.i;

err:
	return clr;
}

static int label_color(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	uint clr;

	if ((uint)-1 == (clr = ffpic_color3(val->ptr, val->len, ffpic_clr_a)))
		return FFUI_EINVAL;

	g->actl.lbl->color = clr;
	return 0;
}

static int label_cursor(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_ieqcz(val, "hand"))
		ffui_lbl_setcursor(g->actl.lbl, FFUI_CUR_HAND);
	else
		return FFUI_EINVAL;
	return 0;
}

static int label_action(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->actl.lbl->click_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg label_args[] = {
	{ "text",		T_STR,		_F(label_text) },
	{ "style",		T_STRLIST,	_F(ctl_style) },
	{ "font",		T_OBJ,		_F(label_font) },
	{ "color",		T_STR,		_F(label_color) },
	{ "cursor",		T_STR,		_F(label_cursor) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ "onclick",	T_STR,		_F(label_action) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};
static int new_label(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_lbl_create(g->actl.lbl, g->wnd))
		return FFUI_ENOMEM;

	state_reset2(g);
	ffconf_scheme_addctx(cs, label_args, g);
	return 0;
}


static int image_icon(ffconf_scheme *cs, ffui_loader *g)
{
	ffmem_zero_obj(&g->ico_ctl);
	g->ico_ctl.ldr = g;
	state_reset(g);
	ffconf_scheme_addctx(cs, icon_args, &g->ico_ctl);
	return 0;
}
static int image_action(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->actl.img->click_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static int image_done(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->ico_ctl.icon.h != NULL)
		ffui_img_set(g->actl.img, &g->ico_ctl.icon);
	ffui_show(g->ctl, 1);
	return 0;
}
static const ffconf_arg image_args[] = {
	{ "style",		T_STRLIST,	_F(ctl_style) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "icon",		T_OBJ,		_F(image_icon) },
	{ "onclick",	T_STR,		_F(image_action) },
	{ NULL,			T_CLOSE,	_F(image_done) },
};
static int new_image(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_img_create(g->actl.img, g->wnd))
		return FFUI_ENOMEM;

	state_reset(g);
	ffconf_scheme_addctx(cs, image_args, g);
	return 0;
}


static int ctl_tooltip(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_wnd_tooltip(g->wnd, g->ctl, val->ptr, val->len);
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
	if (g->ico_ctl.icon.h != NULL)
		ffui_btn_seticon(g->btn, &g->ico_ctl.icon);

	ctl_done(cs, g);
	return 0;
}
static const ffconf_arg btn_args[] = {
	{ "text",		T_STR,		_F(label_text) },
	{ "style",		T_STRLIST,	_F(ctl_style) },
	{ "icon",		T_OBJ,		_F(image_icon) },
	{ "font",		T_OBJ,		_F(label_font) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ "tooltip",	T_STR,		_F(ctl_tooltip) },
	{ "action",		T_STR,		_F(btn_action) },
	{ NULL,			T_CLOSE,	_F(btn_done) },
};
static int new_button(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_btn_create(g->ctl, g->wnd))
		return FFUI_ENOMEM;

	ffmem_zero_obj(&g->ico_ctl);
	state_reset2(g);
	ffconf_scheme_addctx(cs, btn_args, g);
	return 0;
}


static int chbox_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "checked"))
		ffui_chbox_check(g->btn, 1);
	else
		return ctl_style(cs, g, val);
	return 0;
}
static const ffconf_arg chbox_args[] = {
	{ "text",		T_STR,		_F(label_text) },
	{ "style",		T_STRLIST,	_F(chbox_style) },
	{ "font",		T_OBJ,		_F(label_font) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ "tooltip",	T_STR,		_F(ctl_tooltip) },
	{ "action",		T_STR,		_F(btn_action) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};
static int new_checkbox(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_chbox_create(g->ctl, g->wnd))
		return FFUI_ENOMEM;

	state_reset(g);
	ffconf_scheme_addctx(cs, chbox_args, g);
	return 0;
}


static int edit_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "password"))
		ffui_edit_password(g->ctl, 1);

	else if (ffstr_eqcz(val, "readonly"))
		ffui_edit_readonly(g->ctl, 1);

	else
		return ctl_style(cs, g, val);

	return 0;
}
static int edit_action(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->ed->change_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg editbox_args[] = {
	{ "text",		T_STR,		_F(label_text) },
	{ "style",		T_STRLIST,	_F(edit_style) },
	{ "font",		T_OBJ,		_F(label_font) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ "tooltip",	T_STR,		_F(ctl_tooltip) },
	{ "onchange",	T_STR,		_F(edit_action) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};
static int new_editbox(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	int r;
	if (ffsz_eq(cs->arg->name, "text"))
		r = ffui_text_create(g->ctl, g->wnd);
	else
		r = ffui_edit_create(g->ctl, g->wnd);
	if (r != 0)
		return FFUI_ENOMEM;

	state_reset2(g);
	ffconf_scheme_addctx(cs, editbox_args, g);
	return 0;
}


static const ffconf_arg combx_args[] = {
	{ "text",		T_STR,		_F(label_text) },
	{ "style",		T_STRLIST,	_F(ctl_style) },
	{ "font",		T_OBJ,		_F(label_font) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};

static int new_combobox(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_combx_create(g->ctl, g->wnd))
		return FFUI_ENOMEM;

	state_reset(g);
	ffconf_scheme_addctx(cs, combx_args, g);
	return 0;
}


static const ffconf_arg radio_args[] = {
	{ "text",		T_STR,		_F(label_text) },
	{ "style",		T_STRLIST,	_F(chbox_style) },
	{ "font",		T_OBJ,		_F(label_font) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ "tooltip",	T_STR,		_F(ctl_tooltip) },
	{ "action",		T_STR,		_F(btn_action) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};
static int new_radio(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_radio_create(g->ctl, g->wnd))
		return FFUI_ENOMEM;

	state_reset(g);
	ffconf_scheme_addctx(cs, radio_args, g);
	return 0;
}


static int tab_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "multiline"))
		ffui_styleset(g->actl.tab->h, TCS_MULTILINE);
	else if (ffstr_eqcz(val, "fixed-width"))
		ffui_styleset(g->actl.tab->h, TCS_FIXEDWIDTH);
	else
		return ctl_style(cs, g, val);
	return 0;
}
static int tab_onchange(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->actl.tab->chsel_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg tab_args[] = {
	{ "style",		T_STRLIST,	_F(tab_style) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ "font",		T_OBJ,		_F(label_font) },
	{ "onchange",	T_STR,		_F(tab_onchange) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};
static int new_tab(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->actl.tab = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_tab_create(g->actl.tab, g->wnd))
		return FFUI_ENOMEM;
	state_reset(g);
	ffconf_scheme_addctx(cs, tab_args, g);
	return 0;
}


enum {
	VIEW_STYLE_CHECKBOXES,
	VIEW_STYLE_EDITLABELS,
	VIEW_STYLE_EXPLORER_THEME,
	VIEW_STYLE_FULL_ROW_SELECT,
	VIEW_STYLE_GRID_LINES,
	VIEW_STYLE_HAS_BUTTONS,
	VIEW_STYLE_HAS_LINES,
	VIEW_STYLE_HORIZ,
	VIEW_STYLE_MULTI_SELECT,
	VIEW_STYLE_TRACK_SELECT,
	VIEW_STYLE_VISIBLE,
};
static const char *const view_styles[] = {
	"checkboxes",
	"edit_labels",
	"explorer_theme",
	"full_row_select",
	"grid_lines",
	"has_buttons",
	"has_lines",
	"horizontal",
	"multi_select",
	"track_select",
	"visible",
};

static int view_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (!g->style_reset) {
		g->style_reset = 1;
		// reset to default
		ListView_SetExtendedListViewStyleEx(g->ctl->h, LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES, 0);
	}

	switch (ffszarr_ifindsorted(view_styles, FF_COUNT(view_styles), val->ptr, val->len)) {

	case VIEW_STYLE_VISIBLE:
		break;

	case VIEW_STYLE_EDITLABELS:
		ffui_styleset(g->ctl->h, LVS_EDITLABELS);
		break;

	case VIEW_STYLE_MULTI_SELECT:
		ffui_styleclear(g->ctl->h, LVS_SINGLESEL);
		break;

	case VIEW_STYLE_GRID_LINES:
		ListView_SetExtendedListViewStyleEx(g->ctl->h, LVS_EX_GRIDLINES, LVS_EX_GRIDLINES);
		break;

	case VIEW_STYLE_CHECKBOXES:
		ListView_SetExtendedListViewStyleEx(g->ctl->h, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
		break;

	case VIEW_STYLE_EXPLORER_THEME:
		ffui_view_settheme(g->ctl);
		break;

	case VIEW_STYLE_HORIZ:
		g->style_horizontal = 1;
		break;

	default:
		return FFUI_EINVAL;
	}
	return 0;
}

static int tview_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	switch (ffszarr_ifindsorted(view_styles, FF_COUNT(view_styles), val->ptr, val->len)) {

	case VIEW_STYLE_VISIBLE:
		break;

	case VIEW_STYLE_CHECKBOXES:
		ffui_styleset(g->ctl->h, TVS_CHECKBOXES);
		break;

	case VIEW_STYLE_EXPLORER_THEME:
		ffui_view_settheme(g->ctl);
#if FF_WIN >= 0x0600
		TreeView_SetExtendedStyle(g->ctl->h, TVS_EX_FADEINOUTEXPANDOS, TVS_EX_FADEINOUTEXPANDOS);
#endif
		break;

	case VIEW_STYLE_FULL_ROW_SELECT:
		ffui_styleset(g->ctl->h, TVS_FULLROWSELECT);
		break;

	case VIEW_STYLE_TRACK_SELECT:
		ffui_styleset(g->ctl->h, TVS_TRACKSELECT);
		break;

	case VIEW_STYLE_HAS_LINES:
		ffui_styleset(g->ctl->h, TVS_HASLINES);
		break;

	case VIEW_STYLE_HAS_BUTTONS:
		ffui_styleset(g->ctl->h, TVS_HASBUTTONS);
		break;

	case VIEW_STYLE_HORIZ:
		g->style_horizontal = 1;
		break;

	default:
		return FFUI_EINVAL;
	}
	return 0;
}

static int view_color(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	uint clr;

	if ((uint)-1 == (clr = ffpic_color3(val->ptr, val->len, ffpic_clr_a)))
		return FFUI_EINVAL;

	if (!ffsz_cmp(cs->arg->name, "color"))
		ffui_view_clr_text(g->vi, clr);
	else
		ffui_view_clr_bg(g->vi, clr);
	return 0;
}

static int view_pmenu(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_menu *m = g->getctl(g->udata, val);
	if (m == NULL)
		return FFUI_EINVAL;
	g->vi->pmenu = m;
	return 0;
}

static int view_chsel(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->vi->chsel_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}

static int view_lclick(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	int id = g->getcmd(g->udata, val);
	if (id == 0) return FFUI_EINVAL;

	if (!ffsz_cmp(cs->arg->name, "lclick"))
		g->vi->lclick_id = id;
	else
		g->vi->check_id = id;
	return 0;
}

static int view_dblclick(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->vi->dblclick_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}


static int viewcol_width(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	ffui_viewcol_setwidth(&g->vicol, val);
	return 0;
}

static int viewcol_align(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	uint a;
	if (ffstr_eqcz(val, "left"))
		a = HDF_LEFT;
	else if (ffstr_eqcz(val, "right"))
		a = HDF_RIGHT;
	else if (ffstr_eqcz(val, "center"))
		a = HDF_CENTER;
	else
		return FFUI_EINVAL;
	ffui_viewcol_setalign(&g->vicol, a);
	return 0;
}

static int viewcol_order(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	ffui_viewcol_setorder(&g->vicol, val);
	return 0;
}

static int viewcol_done(ffconf_scheme *cs, ffui_loader *g)
{
	ffui_view_inscol(g->vi, ffui_view_ncols(g->vi), &g->vicol);
	return 0;
}
static const ffconf_arg viewcol_args[] = {
	{ "width",		T_INT32,	_F(viewcol_width) },
	{ "align",		T_STR,		_F(viewcol_align) },
	{ "order",		T_INT32,	_F(viewcol_order) },
	{ NULL,			T_CLOSE,	_F(viewcol_done) },
};

static int view_column(ffconf_scheme *cs, ffui_loader *g)
{
	ffstr *name = ffconf_scheme_objval(cs);
	ffui_viewcol_reset(&g->vicol);
	ffui_viewcol_setwidth(&g->vicol, 100);
	ffstr s = vars_val(&g->vars, *name);
	ffui_viewcol_settext(&g->vicol, s.ptr, s.len);
	state_reset(g);
	ffconf_scheme_addctx(cs, viewcol_args, g);
	return 0;
}

static const ffconf_arg view_args[] = {
	{ "style",		T_STRLIST,	_F(view_style) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "resize",		T_STRLIST,	_F(ctl_resize) },
	{ "color",		T_STR,		_F(view_color) },
	{ "bgcolor",	T_STR,		_F(view_color) },
	{ "popupmenu",	T_STR,		_F(view_pmenu) },

	{ "chsel",		T_STR,		_F(view_chsel) },
	{ "lclick",		T_STR,		_F(view_lclick) },
	{ "dblclick",	T_STR,		_F(view_dblclick) },
	{ "oncheck",	T_STR,		_F(view_lclick) },

	{ "column",		T_OBJMULTI,	_F(view_column) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};

static int new_listview(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_view_create(g->vi, g->wnd))
		return FFUI_ENOMEM;

	state_reset(g);
	ffconf_scheme_addctx(cs, view_args, g);
	return 0;
}

static const ffconf_arg tview_args[] = {
	{ "style",		T_STRLIST,	_F(tview_style) },
	{ "position",	T_INTLIST_S,_F(ctl_pos) },
	{ "size",		T_INTLIST,	_F(ctl_size) },
	{ "color",		T_STR,		_F(view_color) },
	{ "bgcolor",	T_STR,		_F(view_color) },
	{ "popupmenu",	T_STR,		_F(view_pmenu) },

	{ "chsel",		T_STR,		_F(view_chsel) },
	{ NULL,			T_CLOSE,	_F(ctl_done) },
};
static int new_treeview(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->ctl = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	if (0 != ffui_tree_create(g->ctl, g->wnd))
		return FFUI_ENOMEM;

	state_reset(g);
	ffconf_scheme_addctx(cs, tview_args, g);
	return 0;
}


static int pnchild_resize(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "cx"))
		g->paned->items[g->ir - 1].cx = 1;
	else if (ffstr_eqcz(val, "cy"))
		g->paned->items[g->ir - 1].cy = 1;
	else
		return FFUI_EINVAL;
	return 0;
}

static int pnchild_move(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "x"))
		g->paned->items[g->ir - 1].x = 1;
	else if (ffstr_eqcz(val, "y"))
		g->paned->items[g->ir - 1].y = 1;
	else
		return FFUI_EINVAL;
	return 0;
}
static const ffconf_arg paned_child_args[] = {
	{ "move",		T_STRLIST,	_F(pnchild_move) },
	{ "resize",		T_STRLIST,	_F(pnchild_resize) },
	{}
};

static int paned_child(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->ir == FF_COUNT(g->paned->items))
		return FFUI_EINVAL;

	void *ctl = ldr_getctl(g, &cs->objval);
	if (ctl == NULL) return FFUI_EINVAL;

	g->paned->items[g->ir++].it = ctl;
	state_reset(g);
	ffconf_scheme_addctx(cs, paned_child_args, g);
	return 0;
}
static const ffconf_arg paned_args[] = {
	{ "child",	T_OBJMULTI,	_F(paned_child) },
	{}
};

static int new_paned(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->paned = ldr_getctl(g, &cs->objval)))
		return FFUI_EINVAL;

	ffmem_zero_obj(g->paned);
	ffui_paned_create(g->paned, g->wnd);

	g->ir = 0;
	state_reset(g);
	ffconf_scheme_addctx(cs, paned_args, g);
	return 0;
}


// DIALOG
static int dlg_title(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_dlg_title(g->dlg, val->ptr, val->len);
	return 0;
}

static int dlg_filter(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_dlg_filter(g->dlg, val->ptr, val->len);
	return 0;
}
static const ffconf_arg dlg_args[] = {
	{ "title",	T_STR,	_F(dlg_title) },
	{ "filter",	T_STR,	_F(dlg_filter) },
	{}
};
static int new_dlg(ffconf_scheme *cs, ffui_loader *g)
{
	if (NULL == (g->dlg = g->getctl(g->udata, &cs->objval)))
		return FFUI_EINVAL;
	ffui_dlg_init(g->dlg);
	state_reset(g);
	ffconf_scheme_addctx(cs, dlg_args, g);
	return 0;
}


static int wnd_title(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffstr s = vars_val(&g->vars, *val);
	ffui_settextstr(g->wnd, &s);
	return 0;
}

static int wnd_position(ffconf_scheme *cs, ffui_loader *g, int64 v)
{
	int *i = &g->r.x;
	if (g->list_idx == 4)
		return FFUI_EINVAL;
	i[g->list_idx] = (int)v;
	if (g->list_idx == 3) {
		ffui_pos_limit(&g->r, &g->screen);
		ffui_setposrect(g->wnd, &g->r, 0);
	}
	g->list_idx++;
	return 0;
}

static int wnd_placement(ffconf_scheme *cs, ffui_loader *g, int64 v)
{
	int li = g->list_idx++;

	if (li == 0) {
		g->showcmd = v;
		return 0;
	} else if (li == 5)
		return FFUI_EINVAL;

	int *i = &g->r.x;
	i[li - 1] = (int)v;

	if (li == 4) {
		ffui_pos_limit(&g->r, &g->screen);
		ffui_wnd_setplacement(g->wnd, g->showcmd, &g->r);
	}
	return 0;
}

static int wnd_icon(ffconf_scheme *cs, ffui_loader *g)
{
	ffmem_zero(&g->ico, sizeof(_ffui_ldr_icon_t));
	g->ico.ldr = g;
	g->ico.load_small = 1;
	state_reset(g);
	ffconf_scheme_addctx(cs, icon_args, &g->ico);
	return 0;
}

/** 'percent': Opacity value, 10-100 */
static int wnd_opacity(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	uint percent = (uint)val;

	if (!(percent >= 10 && percent <= 100))
		return FFUI_EINVAL;

	ffui_wnd_opacity(g->wnd, percent);
	return 0;
}

static int wnd_borderstick(ffconf_scheme *cs, ffui_loader *g, int64 val)
{
	g->wnd->bordstick = (byte)val;
	return 0;
}

static int wnd_style(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (ffstr_eqcz(val, "visible"))
		g->vis = 1;

	else
		return FFUI_EINVAL;
	return 0;
}

static int wnd_bgcolor(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	uint clr;

	if (ffstr_eqz(val, "null"))
		clr = GetSysColor(COLOR_BTNFACE);
	else if ((uint)-1 == (clr = ffpic_color3(val->ptr, val->len, ffpic_clr_a)))
		return FFUI_EINVAL;
	ffui_wnd_bgcolor(g->wnd, clr);
	return 0;
}

static int wnd_onclose(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	if (0 == (g->wnd->onclose_id = g->getcmd(g->udata, val)))
		return FFUI_EINVAL;
	return 0;
}

static int wnd_popupfor(ffconf_scheme *cs, ffui_loader *g, const ffstr *val)
{
	ffui_ctl *parent = g->getctl(g->udata, val);
	if (parent == NULL) return FFUI_EINVAL;

	SetWindowLongPtr(g->wnd->h, GWLP_HWNDPARENT, (LONG_PTR)parent->h);
	ffui_wnd_setpopup(g->wnd);
	return 0;
}

static int wnd_done(ffconf_scheme *cs, ffui_loader *g)
{
	if (g->ico.icon.h != NULL) {
		ffui_wnd_seticon(g->wnd, &g->ico.icon, &g->ico.icon_small);

	} else {
		ffui_wnd *parent;
		if (NULL != (parent = ffui_ctl_parent(g->wnd))) {
			ffui_icon ico, ico_small;
			ffui_wnd_icon(parent, &ico, &ico_small);
			ffui_wnd_seticon(g->wnd, &ico, &ico_small);
		}
	}

	{
	//main menu isn't visible until the window is resized
	HMENU mm = GetMenu(g->wnd->h);
	if (mm != NULL)
		SetMenu(g->wnd->h, mm);
	}

	if (g->accels.len != 0) {
		int r = ffui_wnd_hotkeys(g->wnd, (void*)g->accels.ptr, g->accels.len);
		g->accels.len = 0;
		if (r != 0)
			return FFUI_ESYS;
	}

	if (g->vis) {
		g->vis = 0;
		ffui_show(g->wnd, 1);
	}

	g->wnd = NULL;
	ffmem_free(g->wndname);  g->wndname = NULL;
	return 0;
}
static const ffconf_arg wnd_args[] = {
	{ "title",		T_STR,		_F(wnd_title) },
	{ "position",	T_INTLIST_S,_F(wnd_position) },
	{ "placement",	T_INTLIST_S,_F(wnd_placement) },
	{ "opacity",	T_INT32,	_F(wnd_opacity) },
	{ "borderstick",FFCONF_TINT8,_F(wnd_borderstick) },
	{ "icon",		T_OBJ,		_F(wnd_icon) },
	{ "style",		T_STRLIST,	_F(wnd_style) },
	{ "popupfor",	T_STR,		_F(wnd_popupfor) },
	{ "font",		T_OBJ,		_F(label_font) },
	{ "bgcolor",	T_STR,		_F(wnd_bgcolor) },
	{ "onclose",	T_STR,		_F(wnd_onclose) },

	{ "mainmenu",	T_OBJ,		_F(new_mmenu) },
	{ "label",		T_OBJMULTI,	_F(new_label) },
	{ "image",		T_OBJMULTI,	_F(new_image) },
	{ "editbox",	T_OBJMULTI,	_F(new_editbox) },
	{ "text",		T_OBJMULTI,	_F(new_editbox) },
	{ "combobox",	T_OBJMULTI,	_F(new_combobox) },
	{ "button",		T_OBJMULTI,	_F(new_button) },
	{ "checkbox",	T_OBJMULTI,	_F(new_checkbox) },
	{ "radiobutton",T_OBJMULTI,	_F(new_radio) },
	{ "trackbar",	T_OBJMULTI,	_F(new_trkbar) },
	{ "progressbar",T_OBJMULTI,	_F(new_pgsbar) },
	{ "tab",		T_OBJMULTI,	_F(new_tab) },
	{ "listview",	T_OBJMULTI,	_F(new_listview) },
	{ "treeview",	T_OBJMULTI,	_F(new_treeview) },
	{ "paned",		T_OBJMULTI,	_F(new_paned) },
	{ "trayicon",	T_OBJ,		_F(new_tray) },
	{ "statusbar",	T_OBJ,		_F(new_stbar) },
	{ NULL,			T_CLOSE,	_F(wnd_done) },
};
static int new_wnd(ffconf_scheme *cs, ffui_loader *g)
{
	ffui_wnd *wnd;

	if (NULL == (wnd = g->getctl(g->udata, &cs->objval)))
		return FFUI_EINVAL;
	ffmem_zero((byte*)g + FF_OFF(ffui_loader, wnd), sizeof(ffui_loader) - FF_OFF(ffui_loader, wnd));
	g->wnd = wnd;
	if (NULL == (g->wndname = ffsz_dupn(cs->objval.ptr, cs->objval.len)))
		return FFUI_ENOMEM;
	g->ctl = (ffui_ctl*)wnd;
	if (0 != ffui_wnd_create(wnd))
		return FFUI_ENOMEM;
	state_reset(g);
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
	{ "*",	T_STRMULTI,	_F(inc_lang_entry) },
	{}
};
static int inc_lang(ffconf_scheme *cs, ffui_loader *g)
{
	ffconf_scheme_addctx(cs, inc_lang_args, g);
	return 0;
}


static const ffconf_arg top_args[] = {
	{ "window",	T_OBJMULTI,	_F(new_wnd) },
	{ "menu",	T_OBJMULTI,	_F(new_menu) },
	{ "dialog",	T_OBJMULTI,	_F(new_dlg) },
	{ "include_language",	T_OBJ,	_F(inc_lang) },
	{}
};

void ffui_ldr_init(ffui_loader *g, ffui_ldr_getctl_t getctl, ffui_ldr_getcmd_t getcmd, void *udata)
{
	ffmem_zero_obj(g);
	ffui_screenarea(&g->screen);
	vars_init(&g->vars);
	g->getctl = getctl;
	g->getcmd = getcmd;
	g->udata = udata;
	vars_init(&g->vars);
}

void ffui_ldr_fin(ffui_loader *g)
{
	vars_free(&g->vars);
	ffvec_free(&g->lang_data);
	ffvec_free(&g->lang_data_def);
	// g->paned_array
	ffvec_free(&g->accels);
	ffmem_free(g->errstr);  g->errstr = NULL;
	ffmem_free(g->wndname);  g->wndname = NULL;
}

static void* ldr_getctl(ffui_loader *g, const ffstr *name)
{
	char buf[255];
	ffstr s;
	s.ptr = buf;
	s.len = ffs_format_r0(buf, sizeof(buf), "%s.%S", g->wndname, name);
	return g->getctl(g->udata, &s);
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

void* ffui_ldr_findctl(const ffui_ldr_ctl *ctx, void *ctl, const ffstr *name)
{
	uint i;
	ffstr s = *name, sctl;

	while (s.len != 0) {
		ffstr_splitby(&s, '.', &sctl, &s);

		for (i = 0; ; i++) {
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
