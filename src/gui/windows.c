/** phiola: Windows-subsystem GUI executor
2023, Simon Zolin */

#include <phiola.h>
#include <track.h>
#include <exe/conf.h>
#include <util/log.h>
#include <util/conf-args.h>
#include <ffsys/process.h>
#include <ffsys/path.h>
#include <ffsys/environ.h>
#include <ffsys/thread.h>
#include <ffsys/globals.h>
#include <ffbase/vector.h>
#include <ffbase/args.h>

struct ctx {
	const phi_core *core;
	const phi_queue_if *queue;
	const phi_ui_if	*uif;

	struct zzlog		log;
	fftime				time_last;
	char				log_date[32];

	char	fn_buf[128], *fn;
	ffstr	root_dir;

	struct ffargs cmd;
	struct conf conf;
	ffvec input; // char*[]
};
static struct ctx *x;

static __thread uint64 thread_id;

static void exe_logv(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, va_list va)
{
	const char *id = (t) ? t->id : NULL;
	const char *ctx = (module || !t) ? module : (char*)x->core->track->cmd(t, PHI_TRACK_CUR_FILTER_NAME);

	if (x->core) {
		ffdatetime dt;
		fftime tm = x->core->time(&dt, PHI_CORE_TIME_LOCAL);
		if (fftime_cmp(&tm, &x->time_last)) {
			x->time_last = tm;
			fftime_tostr1(&dt, x->log_date, sizeof(x->log_date), FFTIME_HMS_MSEC);
		}
	}

	uint64 tid = thread_id;
	if (tid == 0) {
		tid = ffthread_curid();
		thread_id = tid;
	}

	zzlog_printv(log_obj, flags, x->log_date, tid, ctx, id, fmt, va);
}

static void exe_log(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	exe_logv(log_obj, flags, module, t, fmt, va);
	va_end(va);
}

#define errlog(...)  exe_log(&x->log, PHI_LOG_ERR, NULL, NULL, __VA_ARGS__)
#define warnlog(...)  exe_log(&x->log, PHI_LOG_WARN, NULL, NULL, __VA_ARGS__)

static int conf(struct ctx *x)
{
	x->fn = ffmem_alloc(4*1024);
	const char *p;
	if (!(p = ffps_filename(x->fn, 4*1024, NULL)))
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
		.log_level = PHI_LOG_INFO,
		.log = exe_log,
		.logv = exe_logv,
		.log_obj = &x->log,

		.env_expand = env_expand,
		.mod_loading = mod_loading,
		.resource_load = resource_load,

		.workers = ~0U,
		.io_workers = ~0U,
		.timer_interval_msec = 100,

		.code_page = x->conf.codepage_id,
		.root = x->root_dir,
	};
	ffenv_locale(conf.language, sizeof(conf.language), FFENV_LANGUAGE);
	if (!(x->core = phi_core_create(&conf)))
		return -1;
	x->queue = x->core->mod("core.queue");
	return 0;
}

static void gui_log_ctl(uint flags)
{
	if (flags)
		x->log.func = x->uif->log; // GUI is ready to display logs
	else
		x->log.func = NULL;
}

static void logs()
{
	static const char levels[][8] = {
		"ERROR ",
		"WARN  ",
		"INFO  ",
		"INFO  ",
		"INFO  ",
		"DEBUG ",
		"DEBUG+",
	};
	ffmem_copy(x->log.levels, levels, sizeof(levels));
	x->uif = x->core->mod("gui.if");
	struct phi_ui_conf uc = {
		.log_ctl = gui_log_ctl,
	};
	x->uif->conf(&uc);
}

static int input(struct ctx *x, char *s)
{
	*ffvec_pushT(&x->input, char*) = s;
	return 0;
}

static void phi_guigrd_close(void *f, phi_track *t)
{
	x->core->track->stop(t);
}

static int phi_grd_process(void *f, phi_track *t)
{
	if (!x->conf.allow_hibernate)
		x->core->track->filter(t, x->core->mod("core.win-sleep"), 0);
	return PHI_DONE;
}

static const phi_filter phi_guard_gui = {
	NULL, phi_guigrd_close, phi_grd_process,
	"gui-guard"
};

static int action()
{
	const phi_remote_cl_if *rcl = x->core->mod("remote.client");
	if (!rcl->play("gui", *(ffslice*)&x->input, PHI_RCLF_NOLOG)) {
		x->core->sig(PHI_CORE_STOP);
		return 0;
	}

	const phi_remote_sv_if *rsv = x->core->mod("remote.server");
	rsv->start("gui");

	struct phi_queue_conf qc = {
		.first_filter = &phi_guard_gui,
		.ui_module = "gui.track",
	};
	x->queue->create(&qc);

	char **it;
	FFSLICE_WALK(&x->input, it) {
		struct phi_queue_entry qe = {
			.url = *it,
		};
		if (0 == x->queue->add(NULL, &qe))
			x->queue->play(NULL, x->queue->at(NULL, 0));
	}
	ffvec_free(&x->input);

	x->core->mod("gui");
	return 0;
}

static const struct ffarg cmd_args[] = {
	{ "\0\1",		's',	input },
	{ "",			0,		action },
};

static int cmd()
{
	char *cmd_line = ffsz_alloc_wtou(GetCommandLineW());
	int r;
	struct ffargs a = {};
	if ((r = ffargs_process_line(&a, cmd_args, x, FFARGS_O_SKIP_FIRST, cmd_line)) < 0) {
	}
	ffmem_free(cmd_line);
	return r;
}

static void cleanup()
{
	phi_core_destroy();
#ifdef FF_DEBUG
	ffmem_free(x->fn);
	ffmem_free(x);
#endif
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	x = ffmem_new(struct ctx);
	if (conf(x)) goto end;
	if (core()) goto end;
	logs();
	if (cmd()) goto end;
	phi_core_run();

end:
	cleanup();
	return 0;
}
