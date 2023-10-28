/** phiola: GUI
2023, Simon Zolin */

#include <gui/mod.h>
#ifdef FF_WIN
#include <util/gui-winapi/loader.h>
#else
#include <util/gui-gtk/loader.h>
#endif
#ifdef __cplusplus
#include <util/gui.hpp>
#endif
#include <util/gui-loader.h>
#include <ffbase/args.h>

#define USER_CONF_NAME  "gui.conf"

struct gui_wmain;
FF_EXTERN void wmain_init();
FF_EXTERN void wmain_show();
FF_EXTERN void wmain_userconf_write(ffvec *buf);
FF_EXTERN const struct ffarg wmain_args[];

struct gui_winfo;
FF_EXTERN void winfo_init();
FF_EXTERN void winfo_show(uint show, uint idx);

struct gui_wlistadd;
FF_EXTERN void wlistadd_init();
FF_EXTERN void wlistadd_show(uint show);

struct gui_wlistfilter;
FF_EXTERN void wlistfilter_init();
FF_EXTERN void wlistfilter_show(uint show);

FF_EXTERN void wrecord_init();
FF_EXTERN void wrecord_show(uint show);
FF_EXTERN void wrecord_start_stop();
FF_EXTERN void wrecord_userconf_write(ffvec *buf);
FF_EXTERN const struct ffarg wrecord_args[];

struct gui_wconvert;
FF_EXTERN void wconvert_init();
FF_EXTERN void wconvert_show(uint show, ffslice items);
FF_EXTERN void wconvert_set(int id, uint pos);
FF_EXTERN void wconvert_userconf_write(ffvec *buf);
FF_EXTERN const struct ffarg wconvert_args[];

struct gui_wabout;
FF_EXTERN void wabout_init();
FF_EXTERN void wabout_show(uint show);

struct gui {
	ffui_menu mfile
		, mlist
		, mplay
		, mrecord
		, mconvert
		, mhelp;
	ffui_menu mpopup;
	ffui_dialog dlg;
	struct gui_wmain *wmain;
	struct gui_winfo *winfo;
	struct gui_wlistadd *wlistadd;
	struct gui_wlistfilter *wlistfilter;
	struct gui_wrecord *wrecord;
	struct gui_wconvert *wconvert;
	struct gui_wabout *wabout;
};
FF_EXTERN struct gui *gg;

FF_EXTERN void theme_switch();
FF_EXTERN void gui_dragdrop(ffstr data);
FF_EXTERN void file_del(ffslice indexes);
FF_EXTERN void gui_quit();
