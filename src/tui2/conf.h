/** phiola: TUI-ncurses: config
2026, Simon Zolin */

#include <util/conf-args.h>

static char* conf_filename()
{
	return ffsz_allocfmt("%stui.conf", mod->user_conf_dir);
}

struct conf {
	uint active;
	uint color;
	uint play_info_title;
	uint volume;
	char* explorer_dir;
};

#define O(m)  (void*)FF_OFF(struct conf, m)
static const struct ffarg tui2_args[] = {
	{ "active",			'u',	O(active) },
	{ "color",			'u',	O(color) },
	{ "explorer_dir",	'=s',	O(explorer_dir) },
	{ "play_info_title",'u',	O(play_info_title) },
	{ "volume",			'u',	O(volume) },
	{}
};
#undef O

static void conf_save()
{
	struct conf c = {
		.active = mod->list.active_track,
		.color = mod->colors[0],
		.play_info_title = mod->play_info_title,
		.volume = mod->volume,
		.explorer_dir = mod->ex.dir,
	};
	uint f = 0;
#ifdef FF_WIN
	f = FFCONFW_FCRLF;
#endif
	ffconfw cw = {};
	ffconfw_init(&cw, f);
	ffarg_write_conf(&cw, tui2_args, &c);

	char *fn = conf_filename();
	if (fffile_writewhole(fn, cw.buf.ptr, cw.buf.len, 0))
		syserrlog("file write: %s", fn);

	ffconfw_close(&cw);
	ffmem_free(fn);
}

static void conf_load()
{
	struct conf c = {};
	char *fn = conf_filename();
	ffvec buf = {};
	if (fffile_readwhole(fn, &buf, 16*1024)) {
		if (!fferr_notexist(fferr_last()))
			syserrlog("file read: %s", fn);
		goto end;
	}

	struct ffargs as = {};
	int r = ffargs_process_conf(&as, tui2_args, &c, FFARGS_O_DUPLICATES, *(ffstr*)&buf);
	if (r) {
		errlog("reading config %s: %s", fn, as.error);
		goto end;
	}

	mod->colors[0] = c.color;
	mod->list.active_track = c.active;
	mod->ex.dir = c.explorer_dir;
	c.explorer_dir = NULL;
	mod->play_info_title = c.play_info_title;
	mod->volume = ffmin(c.volume, VOL_MAX);

end:
	ffmem_free(c.explorer_dir);
	ffvec_free(&buf);
	ffmem_free(fn);
}
