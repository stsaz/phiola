/** phiola: GUI: Music Library
2026, Simon Zolin */

#include <ffsys/dirscan.h>

struct mlib_row {
	char *path; // eg "/path/list.m3u".  NULL: 'name' holds the directory name
	xxstr name; // eg "list" (points to 'path')
};

#define mlib_row_dirname(it) (!(it)->path)

struct gui_mlib {
	ffui_windowxx	wnd;
	ffui_editxx		tfilter;
	ffui_viewxx		vlist;

	char *wnd_pos;
	uint initialized :1;

	xxvec rows, rows_filtered; // struct mlib_row[]
	char *dirs_cached;
	fftime mtime_cached;

	/** Add playlists from the dir */
	void scan1(const char *dir) {
		struct mlib_row *it = rows.push<struct mlib_row>();
		it->path = NULL;
		char *s = ffsz_allocfmt("[%s]", dir);
		it->name = FFSTR_Z(s);

		ffdirscanx dx = {};
		dx.ds.wildcard = "*.m3u*";
		if (ffdirscanx_open(&dx, dir, FFDIRSCAN_USEWILDCARD | FFDIRSCANX_SORT_DIRS)) {
			syserrlog("dir read: %s", dir);
			return;
		}

		xxstr dirstr(dir);
		const char *fn;
		uint isdir;
		while ((fn = ffdirscanx_next(&dx, &isdir))) {
			if (isdir)
				continue;

			xxstr name, ext;
			ffpath_splitname_str(FFSTR_Z(fn), &name, &ext);
			if (!(ext.equals_i("m3u")
				|| ext.equals_i("m3u8")))
				continue;

			it = rows.push<struct mlib_row>();
			it->path = ffsz_allocfmt("%S%c%s", &dirstr, FFPATH_SLASH, fn);
			it->name.set(it->path + dirstr.len + 1, name.len);
		}
		ffdirscanx_close(&dx);
		dbglog("mlib: scanned %s", dir);
	}

	/** Check if the directories have changed since the last time */
	bool cached(xxstr ds, const char *dirs) {
		fftime mtime_latest = {};
		xxstr d;
		while (ds.len) {
			ds.split_by('\0', &d, &ds);
			fffileinfo fi;
			if (fffile_info_path(d.ptr, &fi))
				continue;
			fftime t = fffileinfo_mtime(&fi);
			if (fftime_cmp_val(mtime_latest, t) < 0)
				mtime_latest = t;
		}

		if (dirs_cached
			&& ffsz_eq(dirs_cached, dirs)
			&& !fftime_cmp_val(mtime_latest, mtime_cached))
			return 1;

		ffmem_free(dirs_cached);
		dirs_cached = ffsz_dup(dirs);
		mtime_cached = mtime_latest;
		return 0;
	}

	/** Add playlists from all ML dirs */
	int scan() {
		const char *dirs = gd->conf.mlib_dirs;
		if (!dirs || !*dirs) {
			vlist.clear();
			vlist.append(xxstr("Please set 'Music Library Directories' in Settings"));
			return 1;
		}

		xxptr dz(ffsz_dup(dirs));
		xxstr ods, ds(dz.ptr), d;
		ods = ds;
		while (ds.len) {
			ds.split_by(';', &d, &ds);
			d.ptr[d.len] = '\0';
		}

		if (this->cached(ods, dirs)) {
			dbglog("mlib: cached: %s", dirs);
			return 0;
		}

		rows.clear();
		ds = ods;
		while (ds.len) {
			ds.split_by('\0', &d, &ds);
			this->scan1(d.ptr);
		}
		return 0;
	}

	/** Create a temporary filtered list */
	void filter(xxstr f) {
		if (!f.len) {
			rows_filtered.free();
			return;
		}

		rows_filtered.clear();
		struct mlib_row *it;
		FFSLICE_WALK(&rows, it) {
			if (mlib_row_dirname(it)
				|| it->name.find_str_i(f) >= 0) {
				struct mlib_row *itf = rows_filtered.push<struct mlib_row>();
				*itf = *it;
			}
		}
	}

	/** Draw rows */
	void refill() {
		const xxvec *v = (rows_filtered.len) ? &rows_filtered : &rows;
		vlist.clear();
		struct mlib_row *it;
		FFSLICE_WALK(v, it) {
			vlist.append(it->name);
		}
	}

	/** Start playing the playlist */
	void play(uint row) {
		const xxvec *v = (rows_filtered.len) ? &rows_filtered : &rows;
		if (row > v->len) return;

		const struct mlib_row *it = v->at<struct mlib_row>(row);
		if (mlib_row_dirname(it))
			return;
		char *s = ffsz_dup(it->path);
		gui_core_task_ptr((void(*)(void*))mlib_play, s);
	}
};

FF_EXTERN const ffui_ldr_ctl wmlib_ctls[] = {
	FFUI_LDR_CTL(gui_mlib, wnd),
	FFUI_LDR_CTL(gui_mlib, tfilter),
	FFUI_LDR_CTL(gui_mlib, vlist),
	FFUI_LDR_CTL_END
};

#define O(m)  (void*)FF_OFF(gui_mlib, m)
const ffarg wmlib_args[] = {
	{ "wmlib.pos",	'=s',	O(wnd_pos) },
	{}
};
#undef O

void wmlib_userconf_write(ffconfw *cw)
{
	gui_mlib *w = gg->wmlib;
	if (w->initialized) {
		w->wnd_pos = wnd_pos_sz(&w->wnd);
	}
	ffarg_write_conf(cw, wmlib_args, w);
	ffmem_free(w->wnd_pos);
}

static void wmlib_action(ffui_window *wnd, int id)
{
	gui_mlib *w = gg->wmlib;
	switch (id) {
	case A_MLIB_FILTER:
		w->filter(xxvec(w->tfilter.text()).str());
		w->refill();
		break;

	case A_MLIB_PLAY: {
		int i = w->vlist.focused();
		if (i >= 0)
			w->play(i);
		break;
	}
	}
}

void wmlib_show(uint show)
{
	gui_mlib *w = gg->wmlib;
	if (gui_dlg_load())
		return;

	if (!show) {
		w->wnd.show(0);
		return;
	}

	if (!w->initialized) {
		w->initialized = 1;

		if (w->wnd_pos)
			conf_wnd_pos_read(&w->wnd, FFSTR_Z(w->wnd_pos));
		ffmem_free(w->wnd_pos);
		w->wnd_pos = NULL;
	}

	w->rows_filtered.free();
	if (!w->scan())
		w->refill();
	w->wnd.show(1);
	w->wnd.present();
}

void wmlib_init()
{
	gui_mlib *w = gui_allocT(gui_mlib);
	w->wnd.hide_on_close = 1;
	w->wnd.on_action = wmlib_action;
	gg->wmlib = w;
}

void wmlib_fin()
{
	gui_mlib *w = gg->wmlib;
	struct mlib_row *it;
	FFSLICE_WALK(&w->rows, it) {
		ffmem_free(it->path);
		if (mlib_row_dirname(it))
			ffmem_free(it->name.ptr);
	}
	ffmem_free(w->dirs_cached);
	w->~gui_mlib();
}