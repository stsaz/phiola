/** phiola: GUI
2023, Simon Zolin */

#include <gui/mod.h>
#ifdef FF_WIN
#include <ffgui/winapi/loader.h>
#else
#include <ffgui/gtk/loader.h>
#endif
#ifdef __cplusplus
#include <ffgui/gui.hpp>
#endif
#include <ffgui/loader.h>
#include <util/conf-write.h>
#include <ffbase/args.h>

struct gui_wmain;
FF_EXTERN void wmain_init();
FF_EXTERN void wmain_show();
FF_EXTERN void wmain_userconf_write(ffconfw *cw);
FF_EXTERN const struct ffarg wmain_args[];
FF_EXTERN void wmain_fin();

struct gui_winfo;
FF_EXTERN void winfo_init();
FF_EXTERN void winfo_show(uint show, uint idx);
FF_EXTERN void winfo_userconf_write(ffconfw *cw);
FF_EXTERN const struct ffarg winfo_args[];

struct gui_wsettings;
FF_EXTERN void wsettings_init();
FF_EXTERN void wsettings_show(uint show);
FF_EXTERN void wsettings_userconf_write(ffconfw *cw);
FF_EXTERN const struct ffarg wsettings_args[];

struct gui_wgoto;
FF_EXTERN void wgoto_init();

struct gui_wlistadd;
FF_EXTERN void wlistadd_init();
FF_EXTERN void wlistadd_show(uint show);

struct gui_wlistfilter;
FF_EXTERN void wlistfilter_init();
FF_EXTERN void wlistfilter_show(uint show);

FF_EXTERN void wrecord_init();
FF_EXTERN void wrecord_show(uint show);
FF_EXTERN void wrecord_start_stop();
FF_EXTERN void wrecord_userconf_write(ffconfw *cw);
FF_EXTERN const struct ffarg wrecord_args[];

struct gui_wconvert;
FF_EXTERN void wconvert_init();
FF_EXTERN void wconvert_show(uint show, ffslice items);
FF_EXTERN void wconvert_set(int id, uint pos);
FF_EXTERN void wconvert_userconf_write(ffconfw *cw);
FF_EXTERN const struct ffarg wconvert_args[];

struct gui_wabout;
FF_EXTERN void wabout_init();
FF_EXTERN void wabout_show(uint show);

struct gui_wlog;
FF_EXTERN void wlog_init();
FF_EXTERN void wlog_userconf_write(ffconfw *cw);
FF_EXTERN const struct ffarg wlog_args[];

struct gui {
#ifdef __cplusplus
	ffui_menuxx
#else
	ffui_menu
#endif
		mfile
		, mlist
		, mplay
		, mrecord
		, mconvert
		, mhelp;
	ffui_menu mpopup;
	ffui_dialog dlg;
	struct gui_wmain*		wmain;
	struct gui_winfo*		winfo;
	struct gui_wsettings*	wsettings;
	struct gui_wgoto*		wgoto;
	struct gui_wlistadd*	wlistadd;
	struct gui_wlistfilter*	wlistfilter;
	struct gui_wrecord*		wrecord;
	struct gui_wconvert*	wconvert;
	struct gui_wabout*		wabout;
	struct gui_wlog*		wlog;
};
FF_EXTERN struct gui *gg;

FF_EXTERN void conf_wnd_pos_read(ffui_window *w, ffstr val);
static inline void conf_wnd_pos_write(ffconfw *cw, const char *name, ffui_window *w)
{
	ffui_pos pos;
	ffui_wnd_placement(w, &pos);
	ffconfw_addf(cw, "%s \"%d %d %u %u\"", name, pos.x, pos.y, pos.cx, pos.cy);
}
FF_EXTERN void theme_switch(uint i);
FF_EXTERN void gui_dragdrop(ffstr data);
FF_EXTERN void file_del(ffslice indexes);
FF_EXTERN void gui_quit();
