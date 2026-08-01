/** phiola: TUI-ncurses: config
2026, Simon Zolin */

#include <ffbase/conf.h>
#include <util/conf-obj.h>
#include <util/conf-write.h>

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
	ffsize n = ffsz_len(mod->ex.dir);
	ffsize cap = ffconf_escape(NULL, 0, mod->ex.dir, n);
	char *ex_dir_esc = ffmem_alloc(cap + 1);
	n = ffconf_escape(ex_dir_esc, cap, mod->ex.dir, n);
	ex_dir_esc[n] = '\0';

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
		, ex_dir_esc
		, mod->colors[0]
		);

	if (fffile_writewhole(fn, d.ptr, d.len, 0))
		syserrlog("file write: %s", fn);

	ffvec_free(&d);
	ffmem_free(fn);
	ffmem_free(ex_dir_esc);
}

static void conf_load()
{
	int rc = 1;
	struct ffconf_obj co = {};
	char *fn = conf_filename();
	ffvec buf = {};
	if (fffile_readwhole(fn, &buf, 16*1024)) {
		rc = 0;
		if (!fferr_notexist(fferr_last()))
			syserrlog("file read: %s", fn);
		goto end;
	}
	ffstr data = *(ffstr*)&buf;

	ffstr s, key = {};
	while (data.len) {
		int r = ffconf_obj_read(&co, &data, &s);
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
	ffconf_obj_fin(&co);
	if (rc)
		errlog("reading config %s", fn);
	ffvec_free(&buf);
	ffmem_free(fn);
}
