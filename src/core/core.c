/** phiola: Core
2023, Simon Zolin */

#include <phiola.h>
#ifdef FF_WIN
#include <util/woeh.h>
#endif
#include <util/util.h>
#include <FFOS/dylib.h>
#include <FFOS/path.h>
#include <FFOS/process.h>
#include <FFOS/ffos-extern.h>
#include <ffbase/vector.h>

static phi_core _core;
phi_core *core;

#define syserrlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR | PHI_LOG_SYS, "core", NULL, __VA_ARGS__)
#define errlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR, "core", NULL, __VA_ARGS__)
#define dbglog(...) \
do { \
	if (core->conf.log_level >= PHI_LOG_DEBUG) \
		core->conf.log(core->conf.log_obj, PHI_LOG_DEBUG, "core", NULL, __VA_ARGS__); \
} while (0)
#define extralog(...) \
do { \
	if (core->conf.log_level >= PHI_LOG_EXTRA) \
		core->conf.log(core->conf.log_obj, PHI_LOG_EXTRA, "core", NULL, __VA_ARGS__); \
} while (0)

#ifndef PHI_VERSION_STR
	#define PHI_VERSION_STR  "2.0-beta0"
#endif

#include <core/worker.h>

extern void tracks_init();
extern void tracks_destroy();
extern void qm_init();
extern void qm_destroy();
#ifdef FF_WIN
extern void win_sleep_init();
extern void win_sleep_destroy();
#else
static void win_sleep_init(){}
static void win_sleep_destroy(){}
#endif

struct core_mod {
	char *name;
	ffdl dl;
	const struct phi_mod *mod;
};

struct core_ctx {
	struct wrk_ctx wx;
	ffvec mods; // struct core_mod[]
#ifdef FF_WIN
	woeh *woeh_obj;
#endif
};

struct core_ctx *cc;

static void core_sig(uint signal)
{
	switch (signal) {
	case PHI_CORE_STOP:
		wrkx_stop(&cc->wx);
		break;
	}
}

extern const phi_filter
	phi_autorec
	, phi_autoplay
	, phi_auto_input, phi_auto_output
	, phi_dir_r
	, phi_file_r, phi_file_w
	, phi_stdin, phi_stdout
#ifdef FF_WIN
	, phi_winsleep
#endif
	;
extern const phi_queue_if phi_queueif;
static const void* core_iface(const char *name)
{
	static const struct map_sz_vptr map[] = {
		{ "auto-input", &phi_auto_input },
		{ "auto-output", &phi_auto_output },
		{ "auto-play", &phi_autoplay },
		{ "auto-rec", &phi_autorec },
		{ "dir-read", &phi_dir_r },
		{ "file-read", &phi_file_r },
		{ "file-write", &phi_file_w },
		{ "queue", &phi_queueif },
		{ "stdin", &phi_stdin },
		{ "stdout", &phi_stdout },
#ifdef FF_WIN
		{ "win-sleep", &phi_winsleep },
#endif
		{}
	};
	return map_sz_vptr_find(map, name);
}

/** Find module */
static struct core_mod* mod_find(ffstr name)
{
	struct core_mod *m;
	FFSLICE_WALK(&cc->mods, m) {
		if (ffstr_eqz(&name, m->name))
			return m;
	}
	return NULL;
}

static void mod_destroy(struct core_mod *m)
{
	if (m == NULL) return;

	if (m->mod != NULL && m->mod->close != NULL) {
		dbglog("%s: closing module", m->name);
		m->mod->close();
	}
	if (m->dl != FFDL_NULL)
		ffdl_close(m->dl);
	ffmem_free(m->name);
}

/** Create module object */
static struct core_mod* mod_create(ffstr name)
{
	struct core_mod *m = ffvec_zpushT(&cc->mods, struct core_mod);
	m->name = ffsz_dupstr(&name);
	return m;
}

static ffdl mod_load(struct core_mod *m, ffstr file)
{
	int done = 0;
	ffdl dl = FFDL_NULL;

	char *fn;
	if (core->conf.root.len)
		fn = ffsz_allocfmt("%Smod%c%S.%s"
			, &core->conf.root, FFPATH_SLASH, &file, FFDL_EXT);
	else
		fn = ffsz_allocfmt("%S.%s", &file, FFDL_EXT);

	if (FFDL_NULL == (dl = ffdl_open(fn, FFDL_SELFDIR))) {
		errlog("%s: ffdl_open: %s", fn, ffdl_errstr());
		goto end;
	}

	phi_mod_init_t mod_init;
	if (NULL == (mod_init = ffdl_addr(dl, "phi_mod_init"))) {
		errlog("%s: ffdl_addr '%s': %s"
			, fn, "phi_mod", ffdl_errstr());
		goto end;
	}

	if (NULL == (m->mod = mod_init(core)))
		goto end;

	uint ma = m->mod->ver/10000
		, mi = m->mod->ver%10000/100
		, pa = m->mod->ver%100;
	dbglog("loaded module %S v%u.%u.%u"
		, &file, ma, mi, pa);

	if (m->mod->ver_core != PHI_VERSION_CORE) {
		errlog("module %S is incompatible with this phiola version", &file);
		goto end;
	}
	done = 1;

end:
	ffmem_free(fn);
	if (!done && dl != FFDL_NULL)
		ffdl_close(dl);
	return dl;
}

/** Load module, get interface */
static const void* core_mod(const char *name)
{
	const void *mi = NULL;
	struct core_mod *m = NULL;

	ffstr s = FFSTR_INITZ(name), file, iface;
	ffstr_splitby(&s, '.', &file, &iface);
	if (!file.len)
		goto end;

	if (ffstr_eqz(&file, "core")) {
		if (NULL == (mi = core_iface(iface.ptr))) {
			errlog("%s: no such interface", name);
			goto end;
		}
		return mi;
	}

	if (NULL == (m = mod_find(file)))
		m = mod_create(file);

	if (m->dl == FFDL_NULL) {
		if (FFDL_NULL == (m->dl = mod_load(m, file)))
			goto end;
	}

	if (!iface.len)
		goto end;

	if (NULL == (mi = m->mod->iface(iface.ptr))) {
		errlog("%s: no such interface", name);
		goto end;
	}

end:
	return mi;
}

static void core_timer(phi_timer *t, int interval_msec, phi_task_func func, void *param)
{
	struct worker *w = ffslice_itemT(&cc->wx.workers, 0, struct worker);
	wrk_timer(w, t, interval_msec, func, param);
}

static void core_task(phi_task *t, phi_task_func func, void *param)
{
	struct worker *w = ffslice_itemT(&cc->wx.workers, 0, struct worker);

	if (func == NULL) {
		fftaskqueue_del(&w->tq, t);
		dbglog("task removed: %p", t);
		return;
	}

	t->handler = func;
	t->param = param;
	zzkq_tq_post(&w->kq_tq, t);
	dbglog("task added: %p", t);
}

static phi_kevent* core_kev_alloc()
{
	struct worker *w = ffslice_itemT(&cc->wx.workers, 0, struct worker);
	phi_kevent *kev = (void*)zzkq_kev_alloc(&w->kq);
	if (!!kev)
		dbglog("kev alloc: %p", kev);
	return kev;
}

static void core_kev_free(phi_kevent *kev)
{
	struct worker *w = ffslice_itemT(&cc->wx.workers, 0, struct worker);
	zzkq_kev_free(&w->kq, (void*)kev);
	dbglog("kev free: %p", kev);
}

static int core_kq_attach(phi_kevent *kev, fffd fd, uint flags)
{
	struct worker *w = ffslice_itemT(&cc->wx.workers, 0, struct worker);
	uint f = FFKQ_READWRITE;
	if (flags == 1)
		f = FFKQ_READ;
	if (!!zzkq_attach(&w->kq, fd, (void*)kev, f)) {
		syserrlog("kq attach");
		return -1;
	}
	dbglog("attached fd:%L to kq:%L, kev:%p", (ffsize)fd, (ffsize)w->kq.kq, kev);
	return 0;
}

#ifdef FF_WIN
static void woeh_task(void *param)
{
	phi_task *t = param;
	core_task(t, t->handler, t->param);
}

static int core_woeh(fffd fd, phi_task *t, phi_task_func func, void *param)
{
	cc->woeh_obj = woeh_create();
	t->handler = func;
	t->param = param;
	return woeh_add(cc->woeh_obj, fd, woeh_task, t);
}
#endif


FF_EXPORT void phi_core_destroy()
{
	if (cc == NULL) return;

	wrkx_destroy(&cc->wx);

	struct core_mod *m;
	FFSLICE_WALK(&cc->mods, m) {
		mod_destroy(m);
	}
	ffvec_free(&cc->mods);

	tracks_destroy();
	qm_destroy();
	win_sleep_destroy();
	ffmem_free(cc);  cc = NULL;
}

static void core_logv(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, va_list va)
{}
static void core_log(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, ...)
{}

FF_EXPORT phi_core* phi_core_create(struct phi_core_conf *conf)
{
	core = &_core;
	core->conf = *conf;
	if (core->conf.log == NULL) {
		core->conf.log = core_log;
		core->conf.logv = core_logv;
	}
	if (!core->conf.code_page) core->conf.code_page = FFUNICODE_WIN1252;
	if (!core->conf.timer_interval_msec) core->conf.timer_interval_msec = 100;

	cc = ffmem_new(struct core_ctx);
	tracks_init();
	qm_init();
	win_sleep_init();

	if (!!wrkx_init(&cc->wx)) {
		ffmem_free(cc);  cc = NULL;
		return NULL;
	}

	return core;
}

FF_EXPORT void phi_core_run()
{
	struct worker *w = ffslice_itemT(&cc->wx.workers, 0, struct worker);
	if (core->conf.run_detach) {
		wrk_start(w);
		return;
	}
	wrk_run(w);
}

extern phi_track_if phi_track_iface;
static phi_core _core = {
	.version_str = PHI_VERSION_STR,
	.track = &phi_track_iface,
	.sig = core_sig,
	.mod = core_mod,
	.kev_alloc = core_kev_alloc,
	.kev_free = core_kev_free,
	.kq_attach = core_kq_attach,
	.timer = core_timer,
	.task = core_task,
#ifdef FF_WIN
	.woeh = core_woeh,
#endif
};
