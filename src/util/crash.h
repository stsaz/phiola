/** Crash handler.
2018, Simon Zolin */

#include <ffsys/backtrace.h>
#include <ffsys/file.h>
#include <ffsys/path.h>
#include <ffsys/process.h>
#include <ffsys/signal.h>
#include <ffsys/thread.h>
#include <ffsys/time.h>

struct crash_info {
	const char *app_name, *full_name;
	const char *dump_file_dir;
	char buf[512], fn[512];
	ffthread_bt bt;

	char **argv;
	unsigned argc;
	const char *cmd_line;

	uint back_trace :1;
	uint print_std_err :1;
	uint strip_paths :1;
};

#ifdef FF_WIN
#define xsz_len  ffwsz_len
#define xs_rfindchar  ffws_rfindchar
#else
#define xsz_len  ffsz_len
#define xs_rfindchar  ffs_rfindchar
#endif

static void _crash_back_trace(struct crash_info *ci, fffd f, unsigned std_err)
{
	ffstr s = FFSTR_INITN(ci->buf, 0);
	unsigned n = ffthread_backtrace(&ci->bt);
	for (unsigned i = 0;  i < n;  i++) {

		const char *frame = ffthread_backtrace_frame(&ci->bt, i);
		const char *modbase = ffthread_backtrace_modbase(&ci->bt, i);
		ffsize offset = frame - modbase;

		const ffsyschar *modname = ffthread_backtrace_modname(&ci->bt, i);
		if (ci->strip_paths) {
			ssize_t k = xs_rfindchar(modname, xsz_len(modname), FFPATH_SLASH);
			if (k > 0)
				modname += k;
		}

		ffstr_addfmt(&s, sizeof(ci->buf), "#%u: 0x%p  %q+0x%xL\n"
			, i, frame, modname, offset);

		fffile_write(f, s.ptr, s.len);
		if (std_err)
			fffile_write(ffstderr, s.ptr, s.len);
		s.len = 0;
	}
}

static inline void crash_handler(struct crash_info *ci, struct ffsig_info *si)
{
	fffd f = FFFILE_NULL;
	int std_err = 0;
	ffstr s = FFSTR_INITN(ci->buf, 0);

	// open file
	fftime t;
	fftime_now(&t);
	if (ffs_format_r0(ci->fn, sizeof(ci->fn), "%s%c%s-crashdump-%xU.txt%Z"
		, ci->dump_file_dir, FFPATH_SLASH, ci->app_name, (int64)t.sec))
		f = fffile_open(ci->fn, FFFILE_CREATENEW | FFFILE_WRITEONLY);
	if (f == FFFILE_NULL) {
		f = ffstderr;
	} else if (ci->print_std_err) {
		std_err = 1;
		ffstr_addfmt(&s, sizeof(ci->buf), "%s crashed: %s\n", ci->app_name, ci->fn);
		fffile_write(ffstderr, s.ptr, s.len);
		s.len = 0;
	}

	// command line
	if (ci->cmd_line || ci->argc) {
		if (ci->cmd_line) {
			s.len = _ffs_copyz(s.ptr, sizeof(ci->buf), ci->cmd_line);
		} else {
			for (unsigned i = 0;  i < ci->argc;  i++) {
				ffstr_addfmt(&s, sizeof(ci->buf), "\"%s\" ", ci->argv[i]);
			}
		}

		ffstr_addchar(&s, sizeof(ci->buf), '\n');
		fffile_write(f, s.ptr, s.len);
		if (std_err)
			fffile_write(ffstderr, s.ptr, s.len);
		s.len = 0;
	}

	// general info
	ffstr_addfmt(&s, sizeof(ci->buf),
		"%s\n"
		"Signal:%u  Address:0x%p  Flags:%xu  Thread:%U\n"
		, ci->full_name
		, si->sig, si->addr, si->flags, ffthread_curid());
	fffile_write(f, s.ptr, s.len);
	if (std_err)
		fffile_write(ffstderr, s.ptr, s.len);
	s.len = 0;

	if (ci->back_trace)
		_crash_back_trace(ci, f, std_err);

	if (f != ffstderr)
		fffile_close(f);
}
