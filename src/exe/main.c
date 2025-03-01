/** phiola: executor
2023, Simon Zolin */

#include <phiola.h>
#include <track.h>
#include <exe/conf.h>
#include <util/log.h>
#include <util/crash.h>
#include <util/conf-args.h>
#include <ffsys/std.h>
#include <ffsys/dylib.h>
#include <ffsys/environ.h>
#include <ffsys/globals.h>

#ifndef FF_DEBUG
#define PHI_CRASH_HANDLER
#endif

struct exe_subcmd {
	void (*cmd_free)(void*);
	int (*action)(void*);
	void *obj;
};

struct exe {
	const phi_core*		core;
	const phi_queue_if*	queue;
	const phi_ui_if*	uif;

	struct zzlog		log;
	fftime				time_last;
	char				log_date[32];

	char	fn_buf[128], *fn;
	char*	cmd_line;
	ffstr	root_dir;
	char*	in_fnames;
	struct conf conf;

	uint		exit_code;
	uint		ctrl_c;
	phi_task	task_stop_all;

	u_char	background, background_child;
	u_char	debug;
	u_char	verbose;
	uint workers;
	uint cpu_affinity;
	uint timer_int_msec;
	uint stdin_busy :1;
	uint stdout_busy :1;
	uint log_file :1;
	uint dont_exit :1;
	uint sum_duration :1;

	uint64 total_dur_msec;

	ffstr codepage;
	struct ffargs cmd;
	struct exe_subcmd subcmd;

	char *dump_file_dir;
	struct crash_info ci;
};
static struct exe *x;

#include <exe/log.h>

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

static void q_on_change(phi_queue_id q, uint flags, uint pos)
{
	if ((flags & 0xff) == '.') { // the whole queue is processed

		if (x->sum_duration) {
			uint64 n = x->total_dur_msec / 1000;
			userlog("Total duration: %U:%02U:%02U\n"
				, n / 3600, (n % 3600) / 60, n % 60);
		}

		if (!x->dont_exit || x->ctrl_c)
			x->core->sig(PHI_CORE_STOP);
	}
}

static void phi_grd_close(void *f, phi_track *t)
{
	if (x->sum_duration && t->audio.format.rate)
		x->total_dur_msec += t->audio.total * 1000 / t->audio.format.rate;

	x->core->track->stop(t);

	if (x->exit_code == ~0U || t->error)
		x->exit_code = t->error & 0xff;

	x->dont_exit = !!(t->chain_flags & PHI_FSTOP); // don't exit app if the last track is stopped by user command
}

static void phi_guigrd_close(void *f, phi_track *t)
{
	x->core->track->stop(t);
}

static int phi_grd_process(void *f, phi_track *t)
{
	if (x->ctrl_c)
		return PHI_ERR; // cancel the tracks that weren't stopped by PHI_TRACK_STOP_ALL handler

	if (!x->conf.allow_hibernate) {
#ifdef FF_LINUX
		x->core->track->filter(t, x->core->mod("dbus.sleep"), 0);
#elif defined FF_WIN
		x->core->track->filter(t, x->core->mod("core.win-sleep"), 0);
#endif
	}

	return PHI_DONE;
}

static const phi_filter phi_guard = {
	NULL, phi_grd_close, phi_grd_process,
	"guard"
};
static const phi_filter phi_guard_gui = {
	NULL, phi_guigrd_close, phi_grd_process,
	"gui-guard"
};


#include <exe/cmd.h>

static int conf(struct exe *x, const char *argv_0)
{
	x->fn = ffmem_alloc(4*1024);
	const char *p;
	if (NULL == (p = ffps_filename(x->fn, 4*1024, argv_0)))
		return -1;

	uint n = ffsz_len(p);
	if (n + 1 <= sizeof(x->fn_buf)) {
		ffmem_copy(x->fn_buf, p, n + 1);
		ffmem_free(x->fn);  x->fn = NULL;
		p = x->fn_buf;
	}

	ffstr s = FFSTR_INITN(p, n);
	if (ffpath_splitpath_str(s, &x->root_dir, NULL) < 0)
		return -1;
	x->root_dir.len++;

	char *conf_fn = ffsz_allocfmt("%Sphiola.conf", &x->root_dir);
	ffvec buf = {};
	if (!fffile_readwhole(conf_fn, &buf, 10*1024*1024)) {
		struct ffargs conf = {};
		int r = ffargs_process_conf(&conf, conf_args, &x->conf, 0, *(ffstr*)&buf);
		if (r)
			warnlog("'%s': %s", conf_fn, conf.error);
	}

	ffvec_free(&buf);
	ffmem_free(conf_fn);
	return 0;
}

static char* env_expand(const char *s)
{
	return ffenv_expand(NULL, NULL, 0, s);
}

static char* mod_loading(ffstr name)
{
	if (ffstr_eqz(&name, "ac-alac"))
		warnlog("ALAC module is deprecated.  Decoding the files from untrusted sources is NOT RECOMMENDED.");

	return ffsz_allocfmt("%Smod%c%S.%s"
		, &x->root_dir, FFPATH_SLASH, &name, FFDL_EXT);
}

static ffstr resource_load(const char *name)
{
	char *fn = ffsz_allocfmt("%Smod%c%s"
		, &x->root_dir, FFPATH_SLASH, name);
	ffvec d = {};
	fffile_readwhole(fn, &d, 100*1024*1024);
	ffmem_free(fn);
	return *(ffstr*)&d;
}

static int core()
{
	struct phi_core_conf conf = {
		.log_level = (x->debug) ? PHI_LOG_EXTRA : ((x->verbose) ? PHI_LOG_VERBOSE : PHI_LOG_INFO),
		.log = exe_log,
		.logv = exe_logv,
		.log_obj = &x->log,

		.env_expand = env_expand,
		.mod_loading = mod_loading,
		.resource_load = resource_load,

		.workers = x->workers,
		.cpu_affinity = x->cpu_affinity,
		.io_workers = ~0U,
		.timer_interval_msec = x->timer_int_msec,

		.code_page = x->conf.codepage_id,
		.root = x->root_dir,
		.stdin_busy = x->stdin_busy,
		.stdout_busy = x->stdout_busy,
	};
	ffenv_locale(conf.language, sizeof(conf.language), FFENV_LANGUAGE);
	if (NULL == (x->core = phi_core_create(&conf)))
		return -1;
	x->queue = x->core->mod("core.queue");
	return 0;
}

static void stop_all(void *param)
{
	if (0 == x->core->track->cmd(NULL, PHI_TRACK_STOP_ALL))
		x->core->sig(PHI_CORE_STOP);
}

static void on_sig(struct ffsig_info *i)
{
	switch (i->sig) {
	case FFSIG_INT:
		x->ctrl_c = 1;
		x->core->task(0, &x->task_stop_all, stop_all, x);

#ifdef FF_WIN
		if (i->flags == CTRL_CLOSE_EVENT) {
			// This is a separate signal handler thread - just wait until main() exits.
			ffthread_sleep(-1);
		}
#endif
		break;

	default:
		crash_handler(&x->ci, i);
	}
}

static void signals(char **argv, int argc, const char *cmd_line)
{
	struct crash_info ci = {
		.app_name = "phiola",
		.full_name = ffsz_allocfmt("phiola v%s (" OS_STR "-" CPU_STR ")", x->core->version_str),
		.dump_file_dir = "/tmp",

		.argv = argv,
		.argc = argc,
		.cmd_line = cmd_line,

		.back_trace = 1,
		.print_std_err = 1,
		.strip_paths = 1,
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
	return x->subcmd.action(x->subcmd.obj);
}

static void cleanup()
{
#ifdef FF_DEBUG
	if (x->subcmd.cmd_free)
		x->subcmd.cmd_free(x->subcmd.obj);
#endif
	phi_core_destroy();
#ifdef FF_DEBUG
	ffmem_free(x->dump_file_dir);
	ffmem_free(x->cmd_line);
	ffmem_free((char*)x->ci.full_name);
	ffmem_free(x->fn);
	ffmem_free(x->in_fnames);
	ffmem_free(x);
#endif
}

int main(int argc, char **argv, char **env)
{
	ffenv_init(NULL, env);
	x = ffmem_new(struct exe);
	x->timer_int_msec = 100;
	x->workers = ~0U;
	x->exit_code = ~0U;
	logs(&x->log);

	if (conf(x, argv[0])) goto end;

#ifdef FF_WIN
	x->cmd_line = ffsz_alloc_wtou(GetCommandLineW());
#endif
	if (cmd(argv, argc, x->cmd_line)) goto end;

	if (x->stdout_busy && !x->log_file)
		logs(&x->log);
	if (core()) goto end;
	version_print();
	signals(argv, argc, x->cmd_line);
	if (jobs()) goto end;
	phi_core_run();

end:
	{
	uint ec = x->exit_code;
	if (ec == ~0U)
		ec = 1;
	dbglog("exit code: %d", ec);
	cleanup();
	return ec;
	}
}
