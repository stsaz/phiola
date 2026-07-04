/** phiola: TUI-ncurses: config
2026, Simon Zolin */

#include <ffbase/conf.h>

static char* conf_filename()
{
	return ffsz_allocfmt("%stui.conf", mod->user_conf_dir);
}

static void conf_save()
{
	char *fn = conf_filename();
	ffvec d = {};
	ffvec_addfmt(&d,
"volume %u\n\
active %u\n\
explorer_dir \"%s\"\n\
"
		, mod->volume
		, mod->list.active_track
		, mod->ex.dir
		);

	if (fffile_writewhole(fn, d.ptr, d.len, 0))
		syserrlog("file write: %s", fn);

	ffvec_free(&d);
	ffmem_free(fn);
}

static void conf_load()
{
	int rc = 1;
	char *fn = conf_filename();
	ffvec buf = {};
	if (fffile_readwhole(fn, &buf, 16*1024)) {
		rc = 0;
		if (!fferr_notexist(fferr_last()))
			syserrlog("file read: %s", fn);
		goto end;
	}
	ffstr data = *(ffstr*)&buf;

	struct ffconf c = {};
	ffstr s, key = {};
	while (data.len) {
		int r = ffconf_read(&c, &data, &s);
		switch (r) {
		case FFCONF_KEY:
			key = s;  break;

		case FFCONF_VAL:
			if (ffstr_eqz(&key, "volume")) {
				ffs_toint(s.ptr, s.len, &mod->volume, FFS_INT8);
				mod->volume = ffmin(mod->volume, VOL_MAX);

			} else if (ffstr_eqz(&key, "active")) {
				ffs_toint(s.ptr, s.len, &mod->list.active_track, FFS_INT32);

			} else if (ffstr_eqz(&key, "explorer_dir")) {
				ffmem_free(mod->ex.dir);
				mod->ex.dir = ffsz_dupstr(&s);
			}
			break;

		case FFCONF_MORE: break;
		default:
			goto end;
		}
	}

	rc = 0;

end:
	if (rc)
		errlog("reading config %s", fn);
	ffvec_free(&buf);
	ffmem_free(fn);
}
