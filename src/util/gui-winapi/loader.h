/** GUI-winapi loader.
2014, Simon Zolin */

#pragma once
#include "winapi.h"
#include "combobox.h"
#include "dialog.h"
#include "edit.h"
#include "menu.h"
#include "tab.h"
#include "tray.h"
#include "tree.h"
#include "view.h"
#include "window.h"
#include "../conf-scheme.h"
#include "../gui-vars.h"


typedef struct ffui_loader ffui_loader;

typedef struct {
	ffui_icon icon;
	ffui_icon icon_small;
	ffstr fn;
	int idx;
	ffui_loader *ldr;
	uint cx;
	uint cy;
	uint resource;
	uint load_small :1;
} _ffui_ldr_icon_t;

typedef void* (*ffui_ldr_getctl_t)(void *udata, const ffstr *name);

/** Get command ID by its name.
Return 0 if not found. */
typedef int (*ffui_ldr_getcmd_t)(void *udata, const ffstr *name);

struct ffui_loader {
	ffui_ldr_getctl_t getctl;
	ffui_ldr_getcmd_t getcmd;
	void *udata;
	uint list_idx;

	char language[2];
	ffvec lang_data_def, lang_data;
	ffmap vars; // hash(name) -> struct var*

	ffvec paned_array; // ffui_paned*[].  User must free the controls and vector manually.
	ffvec accels; //ffui_wnd_hotkey[]
	ffstr path;
	_ffui_ldr_icon_t ico;
	ffui_pos screen;
	// everything below is memzero'ed for each new window
	ffui_wnd *wnd;
	ffui_pos prev_ctl_pos;
	ffui_pos r;
	int ir;
	_ffui_ldr_icon_t ico_ctl;
	union {
		struct {
			ffui_menuitem mi;
			uint iaccel :1;
		} menuitem;
		struct {
			uint show :1;
		} tr;
		ffui_font fnt;
		ffui_viewcol vicol;
		ffvec sb_parts;
	};
	union {
		ffui_ctl *ctl;
		ffui_btn *btn;
		ffui_edit *ed;
		ffui_paned *paned;
		ffui_trkbar *trkbar;
		ffui_view *vi;
		ffui_dialog *dlg;
		ffui_menu *menu;
		ffui_trayicon *tray;

		union ffui_anyctl actl;
	};
	char *errstr;
	char *wndname;
	uint showcmd;
	uint resize_flags;
	uint vis :1;
	uint style_horizontal :1;
	uint auto_pos :1;
	uint man_pos :1;
	uint style_reset :1;
};

/** Initialize GUI loader.
getctl: get a pointer to a UI element by its name.
 Most of the time you just need to call ffui_ldr_findctl() from it.
getcmd: get command ID by its name
udata: user data */
FF_EXTERN void ffui_ldr_init(ffui_loader *g, ffui_ldr_getctl_t getctl, ffui_ldr_getcmd_t getcmd, void *udata);

FF_EXTERN void ffui_ldr_fin(ffui_loader *g);

#define ffui_ldr_errstr(g)  ((g)->errstr)

/** Load GUI from file. */
FF_EXTERN int ffui_ldr_loadfile(ffui_loader *g, const char *fn);


typedef struct ffui_ldr_ctl ffui_ldr_ctl;
struct ffui_ldr_ctl {
	const char *name;
	uint flags; //=offset
	const ffui_ldr_ctl *children;
};

#define FFUI_LDR_CTL(struct_name, ctl) \
	{ #ctl, FF_OFF(struct_name, ctl), NULL }

#define FFUI_LDR_CTL3(struct_name, ctl, children) \
	{ #ctl, FF_OFF(struct_name, ctl), children }
#define FFUI_LDR_CTL3_PTR(struct_name, ctl, children) \
	{ #ctl, 0x80000000 | FF_OFF(struct_name, ctl), children }

#define FFUI_LDR_CTL_END  {NULL, 0, NULL}

/** Find control by its name in structured hierarchy.
@name: e.g. "window.control" */
FF_EXTERN void* ffui_ldr_findctl(const ffui_ldr_ctl *ctx, void *ctl, const ffstr *name);
