/** phiola: GUI
2023, Simon Zolin */

#include <gui/mod.h>
#ifdef FF_WIN
#include <util/gui-winapi/loader.h>
#else
#include <util/gui-gtk/loader.h>
#endif

#define USER_CONF_NAME  "gui.conf"

struct gui_wmain;
FF_EXTERN void wmain_init();
FF_EXTERN void wmain_show();
FF_EXTERN void wmain_userconf_write(ffvec *buf);
FF_EXTERN int wmain_userconf_read(ffstr key, ffstr val);

struct gui_winfo;
FF_EXTERN void winfo_init();
FF_EXTERN void winfo_show(uint show, uint idx);

struct gui_wlistadd;
FF_EXTERN void wlistadd_init();
FF_EXTERN void wlistadd_show(uint show);

struct gui_wconvert;
FF_EXTERN void wconvert_init();
FF_EXTERN void wconvert_show(uint show, ffslice items);
FF_EXTERN void wconvert_userconf_write(ffvec *buf);
FF_EXTERN int wconvert_userconf_read(ffstr key, ffstr val);

struct gui_wabout;
FF_EXTERN void wabout_init();
FF_EXTERN void wabout_show(uint show);

struct gui {
	ffui_menu mfile;
	ffui_menu mlist;
	ffui_menu mplay;
	ffui_menu mconvert;
	ffui_menu mhelp;
	struct gui_wmain *wmain;
	struct gui_wconvert *wconvert;
	struct gui_winfo *winfo;
	struct gui_wlistadd *wlistadd;
	struct gui_wabout *wabout;
};
FF_EXTERN struct gui *gg;

FF_EXTERN void gui_dragdrop(ffstr data);
FF_EXTERN void file_del(ffstr data);
FF_EXTERN void gui_quit();
