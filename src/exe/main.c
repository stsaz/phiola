/** phiola: executor
2023, Simon Zolin */

#include <phiola.h>
#include <track.h>
#include <util/log.h>
#include <util/crash.h>
#include <util/cmdarg.h>
#include <FFOS/std.h>
#include <FFOS/environ.h>
#include <FFOS/ffos-extern.h>

#ifndef FF_DEBUG
#define PHI_CRASH_HANDLER
#endif

struct exe {
	uint exit_code;
	struct zzlog log;
	const phi_core *core;
	const phi_queue_if *queue;
	const phi_meta_if *metaif;
	phi_timer tmr;
	fftime_zone tz;
	char fn[4*1024];
	ffstr root_dir;

	ffbyte debug;
	ffbyte verbose;
	uint codepage_id;
	uint mode_record :1;
	uint stdin_busy :1;
	uint stdout_busy :1;

	ffstr codepage;
	struct cmd_obj cmd;
	void *cmd_data;
	int (*action)();

	char *dump_file_dir;
	struct crash_info ci;
};
static struct exe *x;

#if defined FF_WIN
	#define OS_STR  "windows"
#elif defined FF_BSD
	#define OS_STR  "bsd"
#elif defined FF_APPLE
	#define OS_STR  "macos"
#else
	#define OS_STR  "linux"
#endif

#if defined FF_AMD64
	#define CPU_STR  "amd64"
#elif defined FF_X86
	#define CPU_STR  "x86"
#elif defined FF_ARM64
	#define CPU_STR  "arm64"
#elif defined FF_ARM
	#define CPU_STR  "arm"
#endif

static void version_print()
{
	ffstderr_fmt("φphiola v%s (" OS_STR "-" CPU_STR ")\n"
		, x->core->version_str);
}

static void on_timer(void *param)
{
	(void)param;
	fftime now;
	fftime_now(&now);
	now.sec += FFTIME_1970_SECONDS + x->tz.real_offset;
	ffdatetime dt;
	fftime_split1(&dt, &now);
	fftime_tostr1(&dt, x->log.date, sizeof(x->log.date), FFTIME_HMS_MSEC);
}

/** Check if fd is a terminal */
static int std_console(fffd fd)
{
#ifdef FF_WIN
	DWORD r;
	return GetConsoleMode(fd, &r);

#else
	fffileinfo fi;
	return (0 == fffile_info(fd, &fi)
		&& FFFILE_UNIX_CHAR == (fffileinfo_attr(&fi) & FFFILE_UNIX_TYPEMASK));
#endif
}

static void exe_logv(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, va_list va)
{
	const char *id = (!!t) ? t->id : NULL;
	const char *ctx = (!!module || !t) ? module : (char*)x->core->track->cmd(t, PHI_TRACK_CUR_FILTER_NAME);
	zzlog_printv(log_obj, flags, ctx, id, fmt, va);
}

static void exe_log(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	exe_logv(log_obj, flags, module, t, fmt, va);
	va_end(va);
}

#define errlog(...) \
	exe_log(&x->log, PHI_LOG_ERR, NULL, NULL, __VA_ARGS__)

#define dbglog(...) \
do { \
	if (x->debug) \
		exe_log(&x->log, PHI_LOG_DEBUG, NULL, NULL, __VA_ARGS__); \
} while (0)

static void logs(struct zzlog *l)
{
	static const char levels[][8] = {
		"ERROR ",
		"WARN  ",
		"INFO  ",
		"INFO  ",
		"",
		"DEBUG ",
		"DEBUG+",
	};
	ffmem_copy(l->levels, levels, sizeof(levels));

	if (!x->stdout_busy) {
		l->fd = ffstdout;
#ifdef FF_WIN
		uint stdout_color = !ffstd_attr(ffstdout, FFSTD_VTERM, FFSTD_VTERM);
		l->fd_file = (!stdout_color && !std_console(ffstdout));
#else
		uint stdout_color = std_console(ffstdout);
#endif
		l->use_color = stdout_color;

	} else {

		l->fd = ffstderr;
#ifdef FF_WIN
		uint stderr_color = !ffstd_attr(ffstderr, FFSTD_VTERM, FFSTD_VTERM);
		l->fd_file = (!stderr_color && !std_console(ffstderr));
#else
		uint stderr_color = std_console(ffstderr);
#endif
		l->use_color = stderr_color;
	}

	static const char colors[][8] = {
		/*PHI_LOG_ERR*/	FFSTD_CLR(FFSTD_RED),
		/*PHI_LOG_WARN*/	FFSTD_CLR(FFSTD_YELLOW),
		/*PHI_LOG_USER*/	"",
		/*PHI_LOG_INFO*/	FFSTD_CLR(FFSTD_GREEN),
		/*PHI_LOG_VERBOSE*/	FFSTD_CLR(FFSTD_GREEN),
		/*PHI_LOG_DEBUG*/	"",
		/*PHI_LOG_EXTRA*/	FFSTD_CLR_I(FFSTD_BLUE),
	};
	ffmem_copy(l->colors, colors, sizeof(colors));
}


static void phi_grd_close(void *f, phi_track *t)
{
	x->core->track->stop(t);
	x->exit_code = t->error & 0xff;

	if (x->mode_record) {
		x->core->sig(PHI_CORE_STOP);
		return;
	}

	if (!x->queue->status(NULL))
		x->core->sig(PHI_CORE_STOP);
}

static int phi_grd_process(void *f, phi_track *t)
{
#ifdef FF_LINUX
	x->core->track->filter(t, x->core->mod("dbus.sleep"), 0);
#elif defined FF_WIN
	x->core->track->filter(t, x->core->mod("core.win-sleep"), 0);
#endif
	return PHI_DONE;
}

static phi_filter phi_guard = {
	NULL, phi_grd_close, phi_grd_process,
	"guard"
};


#include <exe/cmd.h>

static int conf(const char *argv_0)
{
	fftime_local(&x->tz);

	const char *p;
	if (NULL == (p = ffps_filename(x->fn, sizeof(x->fn), argv_0)))
		return -1;
	if (ffpath_splitpath_str(FFSTR_Z(p), &x->root_dir, NULL) < 0)
		return -1;
	x->root_dir.len++;
	return 0;
}

static int core()
{
	struct phi_core_conf conf = {
		.log_level = (x->debug) ? PHI_LOG_DEBUG : ((x->verbose) ? PHI_LOG_VERBOSE : PHI_LOG_INFO),
		.log = exe_log,
		.logv = exe_logv,
		.log_obj = &x->log,

		.code_page = x->codepage_id,
		.root = x->root_dir,
		.stdin_busy = x->stdin_busy,
	};
	if (NULL == (x->core = phi_core_create(&conf)))
		return -1;
	x->queue = x->core->mod("core.queue");
	return 0;
}

static void on_sig(struct ffsig_info *i)
{
	switch (i->sig) {
	case FFSIG_INT:
		if (0 == x->core->track->cmd(NULL, PHI_TRACK_STOP_ALL))
			x->core->sig(PHI_CORE_STOP);
		break;
	default:
		crash_handler(&x->ci, i);
	}
}

static void signals()
{
	struct crash_info ci = {
		.app_name = "phiola",
		.full_name = ffsz_allocfmt("phiola v%s (" OS_STR "-" CPU_STR ")", x->core->version_str),
		.dump_file_dir = "/tmp",
		.back_trace = 1,
		.print_std_err = 1,
	};

#ifdef FF_WIN
	if (NULL == (x->dump_file_dir = ffenv_expand(NULL, NULL, 0, "%TMP%")))
		x->dump_file_dir = ffsz_dupstr(&x->root_dir);
	ci.dump_file_dir = x->dump_file_dir;
#endif

	x->ci = ci;

	static const uint sigs[] = {
		FFSIG_INT,
#ifdef PHI_CRASH_HANDLER
		FFSIG_SEGV, FFSIG_ILL, FFSIG_FPE, FFSIG_ABORT,
#endif
	};
	ffsig_subscribe(on_sig, sigs, FF_COUNT(sigs));
}

static int jobs()
{
	return x->action();
}

static void cleanup()
{
	ffmem_free(x->dump_file_dir);
	phi_core_destroy();
}

int main(int argc, char **argv, char **env)
{
	x = ffmem_new(struct exe);
	x->exit_code = 1;
	logs(&x->log);
	if (!!cmd(argv, argc)) goto end;
	if (!!conf(argv[0])) goto end;
	if (x->stdout_busy)
		logs(&x->log);
	on_timer(NULL);
	if (!!core()) goto end;
	version_print();
	x->core->timer(&x->tmr, 100, on_timer, NULL);
	signals();
	if (!!jobs()) goto end;
	phi_core_run();

end:
	cleanup();
	dbglog("exit code: %d", x->exit_code);
	return x->exit_code;
}
