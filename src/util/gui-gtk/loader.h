/** GUI based on GTK+
2019, Simon Zolin */

#pragma once
#include "gtk.h"
#include "dialog.h"
#include "view.h"
#include "window.h"
#include "../gui-vars.h"
#include <ffbase/vector.h>

typedef void* (*ffui_ldr_getctl_t)(void *udata, const ffstr *name);

/** Get command ID by its name.
Return 0 if not found. */
typedef int (*ffui_ldr_getcmd_t)(void *udata, const ffstr *name);

typedef struct {
	char *fn;
} _ffui_ldr_icon;

typedef struct ffui_loader {
	ffui_ldr_getctl_t getctl;
	ffui_ldr_getcmd_t getcmd;
	void *udata;
	ffstr path;
	ffvec accels; //ffui_wnd_hotkey[]

	char language[2];
	ffvec lang_data_def, lang_data;
	ffmap vars; // hash(name) -> struct var*

	_ffui_ldr_icon ico;
	_ffui_ldr_icon ico_ctl;
	ffui_pos r;
	ffui_wnd *wnd;
	ffui_viewcol vicol;
	ffui_menu *menu;
	void *mi;
	GtkWidget *hbox;
	uint list_idx;
	union {
		ffui_ctl *ctl;
		ffui_label *lbl;
		ffui_btn *btn;
		ffui_checkbox *cb;
		ffui_edit *edit;
		ffui_text *text;
		ffui_trkbar *trkbar;
		ffui_tab *tab;
		ffui_view *vi;
		ffui_image *img;
		ffui_trayicon *trayicon;
		ffui_dialog *dlg;
	};

	char *errstr;
	char *wndname;

	union {
	uint flags;
	struct {
		uint f_horiz :1;
		uint f_loadconf :1; // ffui_ldr_loadconf()
	};
	};
} ffui_loader;

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
	{ #ctl, (ffuint)FF_OFF(struct_name, ctl), NULL }

#define FFUI_LDR_CTL3(struct_name, ctl, children) \
	{ #ctl, (ffuint)FF_OFF(struct_name, ctl), children }
#define FFUI_LDR_CTL3_PTR(struct_name, ctl, children) \
	{ #ctl, 0x80000000 | (ffuint)FF_OFF(struct_name, ctl), children }

#define FFUI_LDR_CTL_END  {NULL, 0, NULL}

/** Find control by its name in structured hierarchy.
@name: e.g. "window.control" */
FF_EXTERN void* ffui_ldr_findctl(const ffui_ldr_ctl *ctx, void *ctl, const ffstr *name);
