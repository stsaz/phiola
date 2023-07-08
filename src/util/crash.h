/** Crash handler.
2018, Simon Zolin */

#include <FFOS/backtrace.h>
#include <FFOS/file.h>
#include <FFOS/path.h>
#include <FFOS/process.h>
#include <FFOS/signal.h>
#include <FFOS/thread.h>
#include <FFOS/time.h>

struct crash_info {
	const char *app_name, *full_name;
	const char *dump_file_dir;
	char buf[512], fn[512];
	ffthread_bt bt;
	uint back_trace :1;
	uint print_std_err :1;
};

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

	if (ci->back_trace) {
		int n = ffthread_backtrace(&ci->bt);
		for (int i = 0;  i < n;  i++) {

			const char *frame = ffthread_backtrace_frame(&ci->bt, i);
			const char *modbase = ffthread_backtrace_modbase(&ci->bt, i);
			ffsize offset = frame - modbase;
			const ffsyschar *modname = ffthread_backtrace_modname(&ci->bt, i);
			ffstr_addfmt(&s, sizeof(ci->buf), "#%u: 0x%p +%xL %q [0x%p]\n"
				, i, frame, offset, modname, modbase);

			fffile_write(f, s.ptr, s.len);
			if (std_err)
				fffile_write(ffstderr, s.ptr, s.len);
			s.len = 0;
		}
	}

	if (f != ffstderr)
		fffile_close(f);
}
