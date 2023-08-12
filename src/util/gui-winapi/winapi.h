/** GUI based on Windows API.
2014, Simon Zolin */

#pragma once
#define UNICODE
#define _UNICODE
#include <FFOS/error.h>
#include <FFOS/path.h>
#include <ffbase/vector.h>
#include <commctrl.h>
#include <uxtheme.h>

#ifdef FFGUI_DEBUG
	#include <FFOS/std.h>
	#define _ffui_log fflog
#else
	#define _ffui_log(...)
#endif

// HOTKEYS
// POINT
// CURSOR
// FONT
// ICON
// CONTROL
// FILE OPERATIONS
// BUTTON
// CHECKBOX
// RADIOBUTTON
// LABEL
// IMAGE
// PANED
// STATUS BAR
// TRACKBAR
// PROGRESS BAR
// MESSAGE LOOP

typedef unsigned int uint;
typedef struct ffui_wnd ffui_wnd;

FF_EXTERN int ffui_init(void);
FF_EXTERN void ffui_uninit(void);

FF_EXTERN uint _ffui_dpi;
FF_EXTERN RECT _ffui_screen_area;

#define _ffui_dpi_descale(x)  ((x) * 96 / _ffui_dpi)
#define _ffui_dpi_scale(x)  ((x) * _ffui_dpi / 96)


// POINT
typedef struct ffui_point {
	int x, y;
} ffui_point;

#define ffui_screen2client(ctl, pt)  ScreenToClient((ctl)->h, (POINT*)pt)


// CURSOR
#define ffui_cur_pos(pt)  GetCursorPos((POINT*)pt)
#define ffui_cur_setpos(x, y)  SetCursorPos(x, y)

enum FFUI_CUR {
	FFUI_CUR_ARROW = OCR_NORMAL,
	FFUI_CUR_IBEAM = OCR_IBEAM,
	FFUI_CUR_WAIT = OCR_WAIT,
	FFUI_CUR_HAND = OCR_HAND,
};

/** @type: enum FFUI_CUR. */
#define ffui_cur_set(type)  SetCursor(LoadCursorW(NULL, (wchar_t*)(type)))


// FONT
typedef struct ffui_font {
	LOGFONTW lf;
} ffui_font;

enum FFUI_FONT {
	FFUI_FONT_BOLD = 1,
	FFUI_FONT_ITALIC = 2,
	FFUI_FONT_UNDERLINE = 4,
};

/** Set font attributes.
flags: enum FFUI_FONT */
FF_EXTERN void ffui_font_set(ffui_font *fnt, const ffstr *name, int height, uint flags);

/** Create font.
Return NULL on error. */
static inline HFONT ffui_font_create(ffui_font *fnt)
{
	return CreateFontIndirectW(&fnt->lf);
}


typedef struct ffui_pos {
	int x, y
		, cx, cy;
} ffui_pos;

/** ffui_pos -> RECT */
static inline void ffui_pos_torect(const ffui_pos *p, RECT *r)
{
	r->left = p->x;
	r->top = p->y;
	r->right = p->x + p->cx;
	r->bottom = p->y + p->cy;
}

static inline void ffui_pos_fromrect(ffui_pos *pos, const RECT *rect)
{
	pos->x = rect->left;
	pos->y = rect->top;
	pos->cx = rect->right - rect->left;
	pos->cy = rect->bottom - rect->top;
}

#define ffui_screenarea(r)  SystemParametersInfo(SPI_GETWORKAREA, 0, r, 0)

FF_EXTERN void ffui_pos_limit(ffui_pos *r, const ffui_pos *screen);

static inline void _ffui_dpi_descalepos(ffui_pos *r)
{
	r->x = _ffui_dpi_descale(r->x);
	r->y = _ffui_dpi_descale(r->y);
	r->cx = _ffui_dpi_descale(r->cx);
	r->cy = _ffui_dpi_descale(r->cy);
}


// ICON
typedef struct ffui_icon {
	HICON h;
} ffui_icon;

#define ffui_icon_destroy(ico)  DestroyIcon((ico)->h)

enum FFUI_ICON_FLAGS {
	FFUI_ICON_DPISCALE = 1,
	FFUI_ICON_SMALL = 2,
};

FF_EXTERN int ffui_icon_load_q(ffui_icon *ico, const wchar_t *filename, uint index, uint flags);
FF_EXTERN int ffui_icon_load(ffui_icon *ico, const char *filename, uint index, uint flags);

/** Load icon with the specified dimensions (resize if needed).
@flags: enum FFUI_ICON_FLAGS */
FF_EXTERN int ffui_icon_loadimg_q(ffui_icon *ico, const wchar_t *filename, uint cx, uint cy, uint flags);
FF_EXTERN int ffui_icon_loadimg(ffui_icon *ico, const char *filename, uint cx, uint cy, uint flags);

enum FFUI_ICON {
	_FFUI_ICON_STD = 0x10000,
	FFUI_ICON_APP = _FFUI_ICON_STD | OIC_SAMPLE,
	FFUI_ICON_ERR = _FFUI_ICON_STD | OIC_ERROR,
	FFUI_ICON_QUES = _FFUI_ICON_STD | OIC_QUES,
	FFUI_ICON_WARN = _FFUI_ICON_STD | OIC_WARNING,
	FFUI_ICON_INFO = _FFUI_ICON_STD | OIC_INFORMATION,
#if (FF_WIN >= 0x0600)
	FFUI_ICON_SHIELD = _FFUI_ICON_STD | OIC_SHIELD,
#endif

	_FFUI_ICON_IMAGERES = 0x20000,
	FFUI_ICON_FILE = _FFUI_ICON_IMAGERES | 2,
	FFUI_ICON_DIR = _FFUI_ICON_IMAGERES | 3,
};

/** Load system standard icon.
@tag: enum FFUI_ICON */
FF_EXTERN int ffui_icon_loadstd(ffui_icon *ico, uint tag);

/** Load icon from resource. */
FF_EXTERN int ffui_icon_loadres(ffui_icon *ico, const wchar_t *name, uint cx, uint cy);


// ICON LIST
typedef struct ffui_iconlist {
	HIMAGELIST h;
} ffui_iconlist;

FF_EXTERN int ffui_iconlist_create(ffui_iconlist *il, uint width, uint height);

#define ffui_iconlist_add(il, ico)  ImageList_AddIcon((il)->h, (ico)->h)

static inline void ffui_iconlist_addstd(ffui_iconlist *il, uint tag)
{
	ffui_icon ico;
	ffui_icon_loadstd(&ico, tag);
	ffui_iconlist_add(il, &ico);
}


// GDI

/** Bit block transfer */
static inline void ffui_bitblt(HDC dst, const ffui_pos *r, HDC src, const ffui_point *ptSrc, uint code)
{
	BitBlt(dst, r->x, r->y, r->cx, r->cy, src, ptSrc->x, ptSrc->y, code);
}

/** Draw text */
static inline void ffui_drawtext_q(HDC dc, const ffui_pos *r, uint fmt, const wchar_t *text, uint len)
{
	RECT rr;
	ffui_pos_torect(r, &rr);
	DrawTextExW(dc, (wchar_t*)text, len, &rr, fmt, NULL);
}


// CONTROL
enum FFUI_UID {
	FFUI_UID_WINDOW = 1,
	FFUI_UID_LABEL,
	FFUI_UID_IMAGE,
	FFUI_UID_EDITBOX,
	FFUI_UID_TEXT,
	FFUI_UID_COMBOBOX,
	FFUI_UID_BUTTON,
	FFUI_UID_CHECKBOX,
	FFUI_UID_RADIO,

	FFUI_UID_TRACKBAR,
	FFUI_UID_PROGRESSBAR,
	FFUI_UID_STATUSBAR,

	FFUI_UID_TAB,
	FFUI_UID_LISTVIEW,
	FFUI_UID_TREEVIEW,
};

#define FFUI_CTL \
	HWND h; \
	enum FFUI_UID uid; \
	const char *name

typedef struct ffui_ctl {
	FFUI_CTL;
	HFONT font;
} ffui_ctl;

FF_EXTERN int _ffui_ctl_create(ffui_ctl *c, enum FFUI_UID uid, HWND parent, uint style, uint exstyle);

#define ffui_getctl(h)  ((void*)GetWindowLongPtrW(h, GWLP_USERDATA))
#define ffui_setctl(h, udata)  SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)(udata))

#define ffui_send(h, msg, w, l)  SendMessageW(h, msg, (size_t)(w), (size_t)(l))
#define ffui_post(h, msg, w, l)  PostMessageW(h, msg, (size_t)(w), (size_t)(l))
#define ffui_ctl_send(c, msg, w, l)  ffui_send((c)->h, msg, w, l)
#define ffui_ctl_post(c, msg, w, l)  ffui_post((c)->h, msg, w, l)

enum FFUI_FPOS {
	FFUI_FPOS_DPISCALE = 1,
	FFUI_FPOS_CLIENT = 2,
	FFUI_FPOS_REL = 4,
};

/**
@flags: enum FFUI_FPOS */
FF_EXTERN void ffui_getpos2(void *ctl, ffui_pos *r, uint flags);
#define ffui_getpos(ctl, r) \
	ffui_getpos2(ctl, r, FFUI_FPOS_DPISCALE | FFUI_FPOS_REL)

static inline int ffui_setpos(void *ctl, int x, int y, int cx, int cy, int flags)
{
	return !SetWindowPos(((ffui_ctl*)ctl)->h, HWND_TOP, _ffui_dpi_scale(x), _ffui_dpi_scale(y)
		, _ffui_dpi_scale(cx), _ffui_dpi_scale(cy), SWP_NOACTIVATE | flags);
}

#define ffui_setposrect(ctl, rect, flags) \
	ffui_setpos(ctl, (rect)->x, (rect)->y, (rect)->cx, (rect)->cy, flags)

static inline int ffui_settext(void *c, const char *text, ffsize len)
{
	wchar_t *w, ws[255];
	ffsize n = FF_COUNT(ws) - 1;
	if (NULL == (w = ffs_utow(ws, &n, text, len)))
		return -1;
	w[n] = '\0';
	int r = ffui_send(((ffui_ctl*)c)->h, WM_SETTEXT, NULL, w);
	if (w != ws)
		ffmem_free(w);
	return r;
}
#define ffui_settextz(c, sz)  ffui_settext(c, sz, ffsz_len(sz))
#define ffui_settextstr(c, str)  ffui_settext(c, (str)->ptr, (str)->len)

#define ffui_textlen(c)  ffui_send((c)->h, WM_GETTEXTLENGTH, 0, 0)
FF_EXTERN int ffui_textstr(void *_c, ffstr *dst);

#define ffui_show(c, show)  ShowWindow((c)->h, (show) ? SW_SHOW : SW_HIDE)

#define ffui_redraw(c, redraw)  ffui_ctl_send(c, WM_SETREDRAW, redraw, 0)

/** Invalidate control */
static inline void ffui_ctl_invalidate(void *ctl)
{
	ffui_ctl *c = (ffui_ctl*)ctl;
	InvalidateRect(c->h, NULL, 1);
}

#define ffui_setfocus(c)  SetFocus((c)->h)

/** Enable or disable the control */
#define ffui_ctl_enable(c, enable)  EnableWindow((c)->h, enable)

FF_EXTERN int ffui_ctl_destroy(void *c);

#define ffui_styleset(h, style_bit) \
	SetWindowLongW(h, GWL_STYLE, GetWindowLongW(h, GWL_STYLE) | (style_bit))

#define ffui_styleclear(h, style_bit) \
	SetWindowLongW(h, GWL_STYLE, GetWindowLongW(h, GWL_STYLE) & ~(style_bit))

/** Get parent control object. */
FF_EXTERN void* ffui_ctl_parent(void *c);

FF_EXTERN int ffui_ctl_setcursor(void *c, HCURSOR h);


// HOTKEYS
typedef uint ffui_hotkey;

/** Parse hotkey string, e.g. "Ctrl+Alt+Shift+Q".
Return: low-word: char key or vkey, hi-word: control flags;  0 on error. */
FF_EXTERN ffui_hotkey ffui_hotkey_parse(const char *s, size_t len);

/** Register global hotkey.
Return 0 on error. */
FF_EXTERN int ffui_hotkey_register(void *ctl, ffui_hotkey hk);

/** Unregister global hotkey. */
static inline void ffui_hotkey_unreg(void *ctl, int id)
{
	UnregisterHotKey(((ffui_ctl*)ctl)->h, id);
	GlobalDeleteAtom(id);
}


// FILE OPERATIONS
typedef struct ffui_fdrop {
	HDROP hdrop;
	uint idx;
	char *fn;
} ffui_fdrop;

#define ffui_fdrop_accept(c, enable)  DragAcceptFiles((c)->h, enable)

FF_EXTERN const char* ffui_fdrop_next(ffui_fdrop *df);


// BUTTON
typedef struct ffui_btn {
	FFUI_CTL;
	HFONT font;
	uint action_id;
} ffui_btn;

FF_EXTERN int ffui_btn_create(ffui_ctl *c, ffui_wnd *parent);

static inline void ffui_btn_seticon(ffui_btn *b, ffui_icon *ico)
{
	ffui_styleset(b->h, BS_ICON);
	ffui_ctl_send(b, BM_SETIMAGE, IMAGE_ICON, ico->h);
}


// CHECKBOX
typedef struct ffui_chbox {
	FFUI_CTL;
	HFONT font;
	uint action_id;
} ffui_chbox;
typedef ffui_chbox ffui_checkbox;

FF_EXTERN int ffui_chbox_create(ffui_ctl *c, ffui_wnd *parent);

#define ffui_chbox_check(c, val)  ffui_ctl_send(c, BM_SETCHECK, val, 0)
#define ffui_chbox_checked(c)  ffui_ctl_send(c, BM_GETCHECK, 0, 0)

#define ffui_checkbox_check(c, val)  ffui_ctl_send(c, BM_SETCHECK, val, 0)
#define ffui_checkbox_checked(c)  ffui_ctl_send(c, BM_GETCHECK, 0, 0)


// RADIOBUTTON
typedef struct ffui_radio {
	FFUI_CTL;
	HFONT font;
	uint action_id;
} ffui_radio;

FF_EXTERN int ffui_radio_create(ffui_ctl *c, ffui_wnd *parent);

#define ffui_radio_check(c)  ffui_chbox_check(c)
#define ffui_radio_checked(c)  ffui_chbox_checked(c)


// LABEL
typedef struct ffui_label {
	FFUI_CTL;
	HFONT font;
	HCURSOR cursor;
	uint color;
	uint click_id;
	uint click2_id;
	WNDPROC oldwndproc;
} ffui_label;

FF_EXTERN int ffui_lbl_create(ffui_label *c, ffui_wnd *parent);

/**
type: enum FFUI_CUR */
static inline void ffui_lbl_setcursor(ffui_label *c, uint type)
{
	ffui_ctl_setcursor(c, LoadCursorW(NULL, (wchar_t*)(size_t)type));
}


// IMAGE
typedef struct ffui_image {
	FFUI_CTL;
	uint click_id;
} ffui_image;

FF_EXTERN int ffui_img_create(ffui_image *im, ffui_wnd *parent);

static inline void ffui_img_set(ffui_image *im, ffui_icon *ico)
{
	ffui_send(im->h, STM_SETIMAGE, IMAGE_ICON, ico->h);
}


// PANED
typedef struct ffui_paned ffui_paned;
struct ffui_paned {
	struct {
		ffui_ctl *it;
		uint x :1
			, y :1
			, cx :1
			, cy :1;
	} items[2];
	ffui_paned *next;
};

FF_EXTERN void ffui_paned_create(ffui_paned *pn, ffui_wnd *parent);


// STATUS BAR
typedef struct ffui_stbar {
	FFUI_CTL;
} ffui_stbar;

FF_EXTERN int ffui_stbar_create(ffui_stbar *c, ffui_wnd *parent);

#define ffui_stbar_setparts(sb, n, parts)  ffui_send((sb)->h, SB_SETPARTS, n, parts)

static inline void ffui_stbar_settext(ffui_stbar *sb, int idx, const char *text, ffsize len)
{
	wchar_t *w, ws[255];
	ffsize n = FF_COUNT(ws) - 1;
	if (NULL == (w = ffs_utow(ws, &n, text, len)))
		return;
	w[n] = '\0';
	ffui_send(sb->h, SB_SETTEXT, idx, w);
	if (w != ws)
		ffmem_free(w);
}
#define ffui_stbar_settextstr(sb, idx, str)  ffui_stbar_settext(sb, idx, (str)->ptr, (str)->len)
#define ffui_stbar_settextz(sb, idx, sz)  ffui_stbar_settext(sb, idx, sz, ffsz_len(sz))


// TRACKBAR
typedef struct ffui_trkbar {
	FFUI_CTL;
	uint scroll_id;
	uint scrolling_id;
	uint thumbtrk :1; //prevent trackbar from updating position while user's holding it
} ffui_trkbar;

FF_EXTERN int ffui_trk_create(ffui_trkbar *t, ffui_wnd *parent);

#define ffui_trk_setrange(t, max)  ffui_ctl_send(t, TBM_SETRANGEMAX, 1, max)

#define ffui_trk_setpage(t, pagesize)  ffui_ctl_send(t, TBM_SETPAGESIZE, 0, pagesize)

static inline void ffui_trk_set(ffui_trkbar *t, uint val)
{
	if (!t->thumbtrk)
		ffui_ctl_send(t, TBM_SETPOS, 1, val);
}

#define ffui_trk_val(t)  ffui_ctl_send(t, TBM_GETPOS, 0, 0)

#define ffui_trk_setdelta(t, delta) \
	ffui_trk_set(t, (int)ffui_trk_val(t) + delta)

enum FFUI_TRK_MOVE {
	FFUI_TRK_PGUP,
	FFUI_TRK_PGDN,
};

/**
cmd: enum FFUI_TRK_MOVE */
static inline void ffui_trk_move(ffui_trkbar *t, uint cmd)
{
	uint pgsize = ffui_ctl_send(t, TBM_GETPAGESIZE, 0, 0);
	uint pos = ffui_trk_val(t);
	switch (cmd) {
	case FFUI_TRK_PGUP:
		pos += pgsize;
		break;
	case FFUI_TRK_PGDN:
		pos -= pgsize;
		break;
	}
	ffui_trk_set(t, pos);
}


// PROGRESS BAR
FF_EXTERN int ffui_pgs_create(ffui_ctl *c, ffui_wnd *parent);

#define ffui_pgs_set(p, val) \
	ffui_ctl_send(p, PBM_SETPOS, val, 0)

#define ffui_pgs_setrange(p, max) \
	ffui_ctl_send(p, PBM_SETRANGE, 0, MAKELPARAM(0, max))


#undef FFUI_CTL

typedef struct ffui_combx ffui_combx;
typedef struct ffui_edit ffui_edit;
typedef struct ffui_menu ffui_menu;
typedef struct ffui_tab ffui_tab;
typedef struct ffui_trayicon ffui_trayicon;
typedef struct ffui_view ffui_view;
union ffui_anyctl {
	ffui_ctl *ctl;
	ffui_label *lbl;
	ffui_image *img;
	ffui_btn *btn;
	ffui_checkbox *cb;
	ffui_edit *edit;
	ffui_combx *combx;
	ffui_paned *paned;
	ffui_trkbar *trkbar;
	ffui_stbar *stbar;
	ffui_tab *tab;
	ffui_view *view;
	ffui_menu *menu;
};


// MESSAGE LOOP
#define ffui_post_quitloop()  PostQuitMessage(0)

FF_EXTERN int ffui_runonce(void);

static inline void ffui_run(void)
{
	while (0 == ffui_runonce())
		;
}

typedef void (*ffui_handler)(void *param);

/** Post a task to the thread running GUI message loop. */
FF_EXTERN void ffui_thd_post(ffui_handler func, void *udata);


#ifdef __cplusplus
struct ffui_trackbarxx : ffui_trkbar {
	void set(uint value) { ffui_trk_set(this, value); }
	uint get() { return ffui_trk_val(this); }
	void range(uint range) { ffui_trk_setrange(this, range); }
};

struct ffui_stbarxx : ffui_stbar {
	void text(const char *sz) { ffui_settextz(this, sz); }
};

struct ffui_labelxx : ffui_label {
	void text(const char *sz) { ffui_settextz(this, sz); }
};
#endif
