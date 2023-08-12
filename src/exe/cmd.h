/** phiola: executor
2023, Simon Zolin */

#include <FFOS/std.h>
#include <FFOS/path.h>
#include <FFOS/dirscan.h>

static const char pcm_fmtstr[][8] = {
	"float32",
	"float64",
	"int16",
	"int24",
	"int24-4",
	"int32",
	"int8",
};
static const ushort pcm_fmt[] = {
	PHI_PCM_FLOAT32,
	PHI_PCM_FLOAT64,
	PHI_PCM_16,
	PHI_PCM_24,
	PHI_PCM_24_4,
	PHI_PCM_32,
	PHI_PCM_8,
};

static const int64 ff_intmasks[9] = {
	0
	, 0xff, 0xffff, 0xffffff, 0xffffffff
	, 0xffffffffffULL, 0xffffffffffffULL, 0xffffffffffffffULL, 0xffffffffffffffffULL
};

static ssize_t ffs_findarr(const void *ar, size_t n, uint elsz, const void *s, size_t len)
{
	if (len <= sizeof(int)) {
		int imask = ff_intmasks[len];
		int left = *(int*)s & imask;
		for (size_t i = 0;  i != n;  i++) {
			if (left == (*(int*)ar & imask) && ((ffbyte*)ar)[len] == 0x00)
				return i;
			ar = (ffbyte*)ar + elsz;
		}
	} else if (len <= sizeof(int64)) {
		int64 left;
		size_t i;
		int64 imask;
		imask = ff_intmasks[len];
		left = *(int64*)s & imask;
		for (i = 0;  i != n;  i++) {
			if (left == (*(int64*)ar & imask) && ((ffbyte*)ar)[len] == 0x00)
				return i;
			ar = (ffbyte*)ar + elsz;
		}
	}
	return -1;
}

#define ffs_findarr3(ar, s, len)  ffs_findarr(ar, FF_COUNT(ar), sizeof(*ar), s, len)

static int pcm_str_fmt(const char *sfmt, size_t len)
{
	int r = ffs_findarr3(pcm_fmtstr, sfmt, len);
	if (r < 0)
		return -1;
	return pcm_fmt[r];
}

static int cmd_time_value(ffuint64 *msec, ffstr s)
{
	ffdatetime dt = {};
	if (s.len != fftime_fromstr1(&dt, s.ptr, s.len, FFTIME_HMS_MSEC_VAR))
		return _ffargs_err(&x->cmd, 1, "incorrect time value '%S'", &s);

	fftime t;
	fftime_join1(&t, &dt);
	*msec = fftime_to_msec(&t);
	return 0;
}

static int cmd_tracks(ffvec *tracks, ffstr s)
{
	while (s.len) {
		ffstr it;
		ffstr_splitby(&s, ',', &it, &s);
		uint n;
		if (!ffstr_to_uint32(&it, &n))
			return _ffargs_err(&x->cmd, 1, "incorrect track number '%S'", &it);
		*ffvec_pushT(tracks, uint) = n;
	}
	return 0;
}

static void cmd_meta_set(ffvec *dst, const ffvec *src)
{
	if (!x->metaif)
		x->metaif = x->core->mod("format.meta");

	ffstr *it;
	FFSLICE_WALK(src, it) {
		ffstr name, val;
		ffstr_splitby(it, '=', &name, &val);
		x->metaif->set(dst, name, val, 0);
	}
}

#ifdef FF_WIN
static int wildcard_expand(ffvec *input, ffstr s)
{
	int rc = -1;
	ffstr dir, name;
	ffpath_splitpath(s.ptr, s.len, &dir, &name);
	if (!dir.len)
		ffstr_setz(&dir, ".");
	char *dirz = ffsz_dupstr(&dir);

	ffdirscan ds = {};
	ds.wildcard = ffsz_dupstr(&name);
	if (!!ffdirscan_open(&ds, dirz, FFDIRSCAN_USEWILDCARD)) {
		errlog("dir open: %s: %s", dirz, fferr_strptr(fferr_last()));
		goto err;
	}

	const char *fn;
	while (!!(fn = ffdirscan_next(&ds))) {
		dbglog("wildcard expand: %s", fn);
		ffstr *p = ffvec_zpushT(input, ffstr);
		ffsize cap = 0;
		ffstr_growfmt(p, &cap, "%S\\%s", &dir, fn);
	}

	rc = 0;

err:
	ffmem_free(dirz);
	ffmem_free((char*)ds.wildcard);
	ffdirscan_close(&ds);
	return rc;
}
#endif

static int cmd_input(ffvec *input, ffstr s)
{
#ifdef FF_WIN
	if (ffstr_findany(&s, "*?", 2) >= 0
		&& !ffstr_matchz(&s, "\\\\?\\")) {
		if (!!wildcard_expand(input, s))
			return 1;
	} else
#endif
	{
		*ffvec_pushT(input, ffstr) = s;
	}
	return 0;
}

#include <exe/record.h>
#include <exe/play.h>
#include <exe/info.h>
#include <exe/convert.h>
#include <exe/device.h>
#include <exe/gui.h>

static int root_help()
{
	static const char s[] = "\
Usage:\n\
    phiola [GLOBAL-OPTIONS] COMMAND [OPTIONS]\n\
\n\
Global options:\n\
  -codepage     Codepage for non-Unicode text:\n\
                  win1251 | win1252\n\
  -Debug        Print debug log messages\n\
\n\
Commands:\n\
  convert   Convert audio\n\
  device    List audio devices\n\
  gui       Show graphical interface\n\
  info      Show file meta data\n\
  play      Play audio [Default command]\n\
  record    Record audio\n\
\n\
'phiola COMMAND -help' will print information on a particular command.\n\
";
	ffstdout_write(s, FFS_LEN(s));
	return 1;
}

static int usage()
{
	static const char s[] = "\
Usage:\n\
	phiola [GLOBAL-OPTIONS] COMMAND [OPTIONS]\n\
Run 'phiola -help' for complete help info.\n\
";
	ffstdout_write(s, FFS_LEN(s));
	return 1;
}

static int ffu_coding(ffstr s)
{
	static const char *const codestr[] = {
		"win866", // FFUNICODE_WIN866
		"win1251", // FFUNICODE_WIN1251
		"win1252", // FFUNICODE_WIN1252
	};
	int r = ffszarr_ifindsorted(codestr, FF_COUNT(codestr), s.ptr, s.len);
	if (r < 0)
		return -1;
	return _FFUNICODE_CP_BEGIN + r;
}

static int cmd_codepage(void *obj, ffstr s)
{
	int r = ffu_coding(s);
	if (r < 0) {
		_ffargs_err(&x->cmd, 1, "unknown codepage: '%S'", &s);
		return 1;
	}
	x->codepage_id = r;
	return 0;
}

#define O(m)  (void*)FF_OFF(struct exe, m)
static const struct ffarg cmd_root[] = {
	{ "-Debug",		'1',		O(debug) },
	{ "-codepage",	's',		cmd_codepage },
	{ "-help",		0,			root_help },
	{ "convert",	'{',		cmd_conv_init },
	{ "device",		'>',		cmd_dev },
	{ "gui",		'{',		cmd_gui_init },
	{ "info",		'{',		cmd_info_init },
	{ "play",		'{',		cmd_play_init },
	{ "record",		'{',		cmd_rec_init },
	{ "\0\1",		'{',		cmd_play_init },
	{ "",			0,			usage },
};
#undef O

static int cmd(char **argv, uint argc)
{
	int r;
	if ((r = ffargs_process_argv(&x->cmd, cmd_root, x, FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES, argv + 1, argc -1)) < 0) {
		errlog("%s", x->cmd.error);
	}

	return r;
}
