/** phiola: TUI-ncurses: config
2026, Simon Zolin */

#include <ffbase/conf.h>

static char* conf_filename()
{
	return ffsz_allocfmt("%stui.conf", mod->user_conf_dir);
}

enum CK {
	CK_ACTIVE,
	CK_COLOR,
	CK_EXPLORER_DIR,
	CK_PLAY_INFO_TITLE,
	CK_VOLUME,
};
static const char ckeys[][16] = {
	"active",
	"color",
	"explorer_dir",
	"play_info_title",
	"volume",
};

static void conf_save()
{
	char *fn = conf_filename();
	ffvec d = {};
	ffvec_addfmt(&d,
"volume %u\n\
play_info_title %u\n\
active %u\n\
explorer_dir \"%s\"\n\
color %u\n\
"
		, mod->volume
		, mod->play_info_title
		, mod->list.active_track
		, mod->ex.dir
		, mod->colors[0]
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

		case FFCONF_VAL: {
			int k = ffcharr_findsorted(ckeys, FF_COUNT(ckeys), sizeof(*ckeys), key.ptr, key.len);
			switch (k) {
			case CK_ACTIVE:
				ffs_toint(s.ptr, s.len, &mod->list.active_track, FFS_INT32);
				break;

			case CK_EXPLORER_DIR:
				ffmem_free(mod->ex.dir);
				mod->ex.dir = ffsz_dupstr(&s);
				break;

			case CK_COLOR:
				ffs_toint(s.ptr, s.len, &mod->colors[0], FFS_INT8);
				break;

			case CK_PLAY_INFO_TITLE:
				if (ffstr_to_uint32(&s, &r))
					mod->play_info_title = !!r;
				break;

			case CK_VOLUME:
				ffs_toint(s.ptr, s.len, &mod->volume, FFS_INT8);
				mod->volume = ffmin(mod->volume, VOL_MAX);
				break;

			default:
				goto end;
			}
			break;
		}

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
