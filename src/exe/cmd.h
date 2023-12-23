/** phiola: executor
2023, Simon Zolin */

#include <ffsys/std.h>
#include <ffsys/path.h>
#include <ffsys/dirscan.h>

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

#define SUBCMD_INIT(ptr, f_free, f_action, args) \
({ \
	x->subcmd.obj = ptr; \
	x->subcmd.cmd_free = (void(*)(void*))f_free; \
	x->subcmd.action = (int(*)(void*))f_action; \
	struct ffarg_ctx cx = { \
		args, x->subcmd.obj \
	}; \
	cx; \
})

#include <exe/convert.h>
#include <exe/device.h>
#include <exe/gui.h>
#include <exe/info.h>
#include <exe/list.h>
#include <exe/play.h>
#include <exe/record.h>
#include <exe/remote.h>
#include <exe/tag.h>

static int root_help()
{
	static const char s[] = "\
Usage:\n\
    phiola [GLOBAL-OPTIONS] COMMAND [OPTIONS]\n\
\n\
Global options:\n\
  -Background   Create new process running in background\n\
  -Codepage     Codepage for non-Unicode text:\n\
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
  remote    Send remote command\n\
  tag       Edit .mp3 file tags\n\
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
	static const char codestr[][8] = {
		"win866", // FFUNICODE_WIN866
		"win1251", // FFUNICODE_WIN1251
		"win1252", // FFUNICODE_WIN1252
	};
	int r = ffcharr_findsorted(codestr, FF_COUNT(codestr), sizeof(codestr[0]), s.ptr, s.len);
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

#ifdef FF_WIN

static inline int ffterm_detach() { return !FreeConsole(); }

// "exe" ARGS -> "exe" arg ARGS
static inline ffps ffps_fork_bg(const char *arg)
{
	ffsize cap, arg_len = ffsz_len(arg), psargs_len, fn_len;
	wchar_t *args, *p, *ps_args, fn[4*1024];

	int r = GetModuleFileNameW(NULL, fn, FF_COUNT(fn));
	if (r == 0 || r == FF_COUNT(fn))
		return FFPS_NULL;
	fn_len = ffwsz_len(fn);

	ps_args = GetCommandLineW();
	psargs_len = ffwsz_len(ps_args);
	if (psargs_len != 0) {
		int i;
		if (ps_args[0] == '"') {
			// '"exe with space" ARGS' -> 'ARGS'
			i = ffs_findstr((char*)(ps_args+1), (psargs_len-1) * sizeof(wchar_t), "\"\0", 2);
			p = ps_args + i/sizeof(wchar_t) + 3;
		} else {
			// 'exe ARGS' -> 'ARGS'
			i = ffs_findstr((char*)ps_args, psargs_len, " \0", 2);
			p = ps_args + i/sizeof(wchar_t) + 1;
		}

		if (i < 0)
			p = ps_args + psargs_len; // incorrect command line

		psargs_len = ps_args + psargs_len - p;
		ps_args = p;
	}

	cap = fn_len+3 + arg_len+1 + psargs_len+1;
	if (NULL == (args = (wchar_t*)ffmem_alloc(cap * sizeof(wchar_t))))
		return FFPS_NULL;
	p = args;

	*p++ = '\"';
	p = ffmem_copy(p, fn, fn_len * sizeof(wchar_t));
	*p++ = '\"';
	*p++ = ' ';

	p += ff_utow(p, args + cap - p, arg, arg_len, 0);
	*p++ = ' ';

	p = ffmem_copy(p, ps_args, psargs_len * sizeof(wchar_t));
	*p++ = '\0';

	ffps_execinfo info = {};
	info.in = info.out = info.err = INVALID_HANDLE_VALUE;
	ffps ps = _ffps_exec_cmdln(fn, args, &info);

	ffmem_free(args);
	return ps;
}

#else

static inline int ffterm_detach()
{
	int f;
	if (-1 == (f = open("/dev/null", O_RDWR)))
		return -1;
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
	if (f > 2)
		close(f);
	return 0;
}

static inline ffps ffps_fork_bg(const char *arg)
{
	(void)arg;

	ffps ps = fork();
	if (ps != 0)
		return ps;

	setsid();
	umask(0);
	return 0;
}

#endif

/** Detach from console */
static int ffterm_detach();

/** Create a copy of the current process in background.
* UNIX: fork; detach from console
* Windows: create new process with 'arg' prepended to its command line
Return
  * child process descriptor (parent);
  * 0 (child);
  * -1 on error */
static ffps ffps_fork_bg(const char *arg);

static int cmd_background()
{
#ifdef FF_WIN
	if (x->background_child)
		goto done;
#endif

	ffps ps = ffps_fork_bg("__bgchild");
	if (ps == FFPS_NULL) {
		syserrlog("spawning background process");
		return 1;

	} else if (ps != 0) {
		ffstdout_fmt("%u\n", ffps_id(ps));
		ffps_close(ps);
		x->exit_code = 0;
		return 1;
	}

#ifdef FF_WIN
done:
#endif
	ffterm_detach();
	return 0;
}

#define O(m)  (void*)FF_OFF(struct exe, m)
static const struct ffarg cmd_root[] = {
	{ "-Background",'1',		O(background) },
	{ "-Codepage",	's',		cmd_codepage },
	{ "-Debug",		'1',		O(debug) },

	{ "-help",		0,			root_help },

	{ "__bgchild",	'1',		O(background_child) },

	{ "convert",	'{',		cmd_conv_init },
	{ "device",		'>',		cmd_dev },
	{ "gui",		'{',		cmd_gui_init },
	{ "info",		'{',		cmd_info_init },
	{ "list",		'>',		cmd_list_args },
	{ "play",		'{',		cmd_play_init },
	{ "record",		'{',		cmd_rec_init },
	{ "remote",		'{',		cmd_remote_init },
	{ "tag",		'{',		cmd_tag_init },
	{ "\0\1",		'{',		cmd_play_init },
	{ "",			0,			usage },
};
#undef O

static int cmd(char **argv, uint argc)
{
	uint f = FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES | FFARGS_O_SKIP_FIRST;
	int r;

#ifdef FF_WIN
	x->cmd_line = ffsz_alloc_wtou(GetCommandLineW());
	r = ffargs_process_line(&x->cmd, cmd_root, x, f, x->cmd_line);

#else
	r = ffargs_process_argv(&x->cmd, cmd_root, x, f, argv, argc);
#endif

	if (r < 0)
		errlog("%s", x->cmd.error);

	if (r == 0
		&& x->background
		&& cmd_background())
		return 1;

	return r;
}
