/** phiola: executor
2023, Simon Zolin */

#include <FFOS/std.h>
#include <FFOS/path.h>

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

#include <exe/record.h>
#include <exe/play.h>
#include <exe/info.h>
#include <exe/convert.h>
#include <exe/device.h>

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
		cmdarg_err(&x->cmd, "unknown codepage: '%S'", &s);
		return 1;
	}
	x->codepage_id = r;
	return 0;
}

#define O(m)  (void*)FF_OFF(struct exe, m)
static const struct cmd_arg cmd_root[] = {
	{ "-Debug",		'1',		O(debug) },
	{ "-codepage",	's',		cmd_codepage },
	{ "-help",		0,			root_help },
	{ "convert",	'{',		cmd_conv_init },
	{ "device",		'>',		cmd_dev },
	{ "info",		'{',		cmd_info_init },
	{ "play",		'{',		cmd_play_init },
	{ "record",		'{',		cmd_rec_init },
	{ "\0\1",		'{',		cmd_play_init },
	{ "",			0,			usage },
};
#undef O

static int cmd(char **argv, uint argc)
{
	struct cmd_obj c = {
		.argv = argv + 1,
		.argc = argc - 1,
		.cx = {
			cmd_root, x
		},
		.options = CMDO_PARTIAL | CMDO_DUPLICATES,
	};
	x->cmd = c;
	int r;
	if ((r = cmd_process(&x->cmd)) < 0) {
		errlog("%s", x->cmd.error);
	}

	return r;
}
