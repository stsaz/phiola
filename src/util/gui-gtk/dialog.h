/** GUI/GTK+: dialogs
2019,2022, Simon Zolin
*/

#pragma once
#include "gtk.h"

typedef struct ffui_dialog ffui_dialog;
struct ffui_dialog {
	char *title;
	char *name;
	GSList *names;
	GSList *curname;
	uint multisel :1;
};

static inline void ffui_dlg_init(ffui_dialog *d)
{
}

static inline void ffui_dlg_destroy(ffui_dialog *d)
{
	g_slist_free_full(d->names, g_free);  d->names = NULL;
	ffmem_free(d->title); d->title = NULL;
	g_free(d->name); d->name = NULL;
}

static inline void ffui_dlg_titlez(ffui_dialog *d, const char *sz)
{
	d->title = ffsz_dup(sz);
}

#define ffui_dlg_multisel(d, val)  ((d)->multisel = (val))

/** Get the next file name (for a dialog with multiselect). */
static inline char* ffui_dlg_nextname(ffui_dialog *d)
{
	if (d->curname == NULL)
		return NULL;
	char *name = (char*)d->curname->data;
	d->curname = d->curname->next;
	return name;
}

static inline char* ffui_dlg_open(ffui_dialog *d, ffui_wnd *parent)
{
	g_free(d->name);  d->name = NULL;

	GtkWidget *dlg;
	GtkWindow *wnd = (GtkWindow*)((ffui_ctl*)parent)->h;
	dlg = gtk_file_chooser_dialog_new(d->title, wnd, GTK_FILE_CHOOSER_ACTION_OPEN
		, "_Cancel", GTK_RESPONSE_CANCEL
		, "_Open", GTK_RESPONSE_ACCEPT
		, NULL);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(dlg);

	if (d->multisel)
		gtk_file_chooser_set_select_multiple(chooser, 1);

	int r = gtk_dialog_run(GTK_DIALOG(dlg));
	if (r == GTK_RESPONSE_ACCEPT) {
		if (d->multisel)
			d->curname = d->names = gtk_file_chooser_get_filenames(chooser);
		else
			d->name = gtk_file_chooser_get_filename(chooser);
	}

	gtk_widget_destroy(dlg);
	if (d->names != NULL)
		return ffui_dlg_nextname(d);
	return d->name;
}

static inline char* ffui_dlg_save(ffui_dialog *d, ffui_wnd *parent, const char *fn, ffsize fnlen)
{
	g_free(d->name);  d->name = NULL;

	GtkWidget *dlg;
	GtkWindow *wnd = (GtkWindow*)((ffui_ctl*)parent)->h;
	dlg = gtk_file_chooser_dialog_new(d->title, wnd, GTK_FILE_CHOOSER_ACTION_SAVE
		, "_Cancel", GTK_RESPONSE_CANCEL
		, "_Save", GTK_RESPONSE_ACCEPT
		, NULL);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(dlg);

	gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

	char *sz = ffsz_dupn(fn, fnlen);
	// gtk_file_chooser_set_filename (chooser, sz);
	gtk_file_chooser_set_current_name(chooser, sz);

	int r = gtk_dialog_run(GTK_DIALOG(dlg));
	if (r == GTK_RESPONSE_ACCEPT) {
		d->name = gtk_file_chooser_get_filename(chooser);
	}

	gtk_widget_destroy(dlg);
	return d->name;
}
