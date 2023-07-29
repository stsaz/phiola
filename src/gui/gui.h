/** phiola: GUI
2023, Simon Zolin */

#include <gui/mod.h>
#include <util/gui-gtk/loader.h>

#define USER_CONF_NAME  "gui.conf"

struct gui_wmain;
FF_EXTERN void wmain_init();
FF_EXTERN void wmain_show();
FF_EXTERN void wmain_userconf_write(ffvec *buf);
FF_EXTERN int wmain_userconf_read(ffstr key, ffstr val);

struct gui {
	ffui_menu mfile;
	ffui_menu mlist;
	ffui_menu mplay;
	struct gui_wmain *wmain;
};
FF_EXTERN struct gui *gg;

FF_EXTERN void gui_dragdrop(ffstr data);
FF_EXTERN void gui_quit();
