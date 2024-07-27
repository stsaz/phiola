/** phiola: Core
2023, Simon Zolin */

#include <phiola.h>
#ifdef FF_WIN
#include <util/woeh.h>
#endif
#include <util/util.h>
#include <ffsys/dylib.h>
#include <ffsys/path.h>
#include <ffsys/process.h>
#include <ffsys/globals.h>
#include <ffbase/vector.h>

static phi_core _core;
phi_core *core;
const phi_meta_if *phi_metaif;

#define syserrlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR | PHI_LOG_SYS, "core", NULL, __VA_ARGS__)
#define errlog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR, "core", NULL, __VA_ARGS__)
#define infolog(...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_INFO, "core", NULL, __VA_ARGS__)
#define dbglog(...) \
do { \
	if (ff_unlikely(core->conf.log_level >= PHI_LOG_DEBUG)) \
		core->conf.log(core->conf.log_obj, PHI_LOG_DEBUG, "core", NULL, __VA_ARGS__); \
} while (0)
#define extralog(...) \
do { \
	if (ff_unlikely(core->conf.log_level >= PHI_LOG_EXTRA)) \
		core->conf.log(core->conf.log_obj, PHI_LOG_EXTRA, "core", NULL, __VA_ARGS__); \
} while (0)

#ifndef PHI_VERSION_STR
	#define PHI_VERSION_STR  "2.2-beta3"
#endif

static int core_wrk_creating(uint iw);

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
	char name[32];
	ffdl dl;
	const struct phi_mod *mod;
};

struct core_ctx {
	fftime_zone tz;
	struct wrk_ctx wx;

	struct zzkcq kcq;
	uint kcq_lazy_start :1;

	fflock mods_lock;
	ffvec mods; // struct core_mod[]
#ifdef FF_WIN
	woeh *woeh_obj;
#endif
};

struct core_ctx *cc;

static void core_sig(uint signal)
{
	dbglog("signal: %u", signal);
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
	, phi_tee
#ifdef FF_WIN
	, phi_winsleep
#endif
	;
extern const phi_queue_if phi_queueif;
static const void* core_iface(const char *name)
{
	static const struct map_sz_vptr map[] = {
		{ "auto-input",	&phi_auto_input },
		{ "auto-output",&phi_auto_output },
		{ "auto-play",	&phi_autoplay },
		{ "auto-rec",	&phi_autorec },
		{ "dir-read",	&phi_dir_r },
		{ "file-read",	&phi_file_r },
		{ "file-write",	&phi_file_w },
		{ "queue",		&phi_queueif },
		{ "stdin",		&phi_stdin },
		{ "stdout",		&phi_stdout },
		{ "tee",		&phi_tee },
#ifdef FF_WIN
		{ "win-sleep",	&phi_winsleep },
#endif
	};
	return map_sz_vptr_findz2(map, FF_COUNT(map), name);
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
		dbglog("'%s': closing module", m->name);
		m->mod->close();
	}
	if (m->dl != FFDL_NULL) {
		dbglog("'%s': ffdl_close", m->name);
		ffdl_close(m->dl);
	}
}

/** Create module object */
static struct core_mod* mod_create(ffstr name)
{
	struct core_mod *m = ffvec_zpushT(&cc->mods, struct core_mod);
	ffsz_copystr(m->name, sizeof(m->name), &name);
	return m;
}

static int mod_load(struct core_mod *m, ffstr file)
{
	int rc = 1;
	ffdl dl = FFDL_NULL;

	fftime t1;
	if (core->conf.log_level >= PHI_LOG_DEBUG)
		t1 = core->time(NULL, PHI_CORE_TIME_MONOTONIC);

	char *fn;
	if (NULL == (fn = core->conf.mod_loading(file))) {
		errlog("%S: module load", &file);
		goto end;
	}

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

	dbglog("%s: calling phi_mod_init()", fn);

	const phi_mod *mod;
	if (NULL == (mod = mod_init(core)))
		goto end;

	if (core->conf.log_level >= PHI_LOG_DEBUG) {
		fftime t2 = core->time(NULL, PHI_CORE_TIME_MONOTONIC);
		fftime_sub(&t2, &t1);

		uint ma = mod->ver/10000
			, mi = mod->ver%10000/100
			, pa = mod->ver%100;
		dbglog("loaded module %S v%u.%u.%u in %Uusec"
			, &file, ma, mi, pa, fftime_to_usec(&t2));
	}

	if (mod->ver_core != PHI_VERSION_CORE) {
		errlog("module %S is incompatible with this phiola version", &file);
		goto end;
	}
	m->mod = mod;
	m->dl = dl;
	dl = FFDL_NULL;
	rc = 0;

end:
	ffmem_free(fn);
	if (dl != FFDL_NULL)
		ffdl_close(dl);
	return rc;
}

/** Find or load module; wait while the module is being loaded by another thread.
Thread-safe.
Return NULL if failed to load. */
static const phi_mod* core_mod_provide(ffstr file)
{
	for (uint ntry = 0;  ;  ntry++) {
		int load = 0;
		struct core_mod *m;

		fflock_lock(&cc->mods_lock);
		if (NULL == (m = mod_find(file))) {
			m = mod_create(file);
			m->dl = (void*)-1;
			load = 1;
		} else if (m->dl == FFDL_NULL) {
			m->dl = (void*)-1;
			load = 1;
		}
		const phi_mod *mod = m->mod;
		uint index = m - (struct core_mod*)cc->mods.ptr;
		fflock_unlock(&cc->mods_lock);

		if (mod)
			return mod;

		if (load) {
			struct core_mod lm;
			int e = mod_load(&lm, file);

			fflock_lock(&cc->mods_lock);
			if (ff_unlikely(e)) {
				m->dl = FFDL_NULL;
				fflock_unlock(&cc->mods_lock);
				return NULL;
			}
			m = ffslice_itemT(&cc->mods, index, struct core_mod);
			m->dl = lm.dl;
			m->mod = lm.mod;
			fflock_unlock(&cc->mods_lock);
			return lm.mod;
		}

		if (ntry < 10) {
			ffthread_yield();
		} else {
			ffthread_sleep(1);
			dbglog("%S: module is being loaded", &file);
		}
	}
}

/** Load module, get interface */
static const void* core_mod(const char *name)
{
	const void *mi = NULL;

	ffstr s = FFSTR_INITZ(name), file, iface;
	ffstr_splitby(&s, '.', &file, &iface);
	if (!file.len || file.len >= sizeof(((struct core_mod*)NULL)->name)) {
		FF_ASSERT(0); // Empty or too large module name
		goto end;
	}

	if (ffstr_eqz(&file, "core")) {
		if (NULL == (mi = core_iface(iface.ptr))) {
			errlog("%s: no such interface", name);
			goto end;
		}
		return mi;
	}

	const phi_mod *mod = core_mod_provide(file);
	if (!mod)
		goto end;

	if (iface.len && NULL == (mi = mod->iface(iface.ptr))) {
		errlog("%s: no such interface", name);
		goto end;
	}

end:
	return mi;
}

static fftime core_time(ffdatetime *dt, uint flags)
{
	fftime t = {};

	switch (flags) {
	case PHI_CORE_TIME_UTC:
	case PHI_CORE_TIME_LOCAL:
		fftime_now(&t);
		t.sec += FFTIME_1970_SECONDS;

		if (flags == PHI_CORE_TIME_LOCAL)
			t.sec += cc->tz.real_offset;

		if (dt)
			fftime_split1(dt, &t);
		break;

	case PHI_CORE_TIME_MONOTONIC:
		t = fftime_monotonic();  break;
	}

	return t;
}

static void core_timer(uint worker, phi_timer *t, int interval_msec, phi_task_func func, void *param)
{
	PHI_ASSERT(worker < cc->wx.workers.len);
	struct worker *w = ffslice_itemT(&cc->wx.workers, worker, struct worker);
	wrk_timer(w, (fftimerqueue_node*)t, interval_msec, func, param);
}

static void core_task(uint worker, phi_task *pt, phi_task_func func, void *param)
{
	PHI_ASSERT(worker < cc->wx.workers.len);
	struct worker *w = ffslice_itemT(&cc->wx.workers, worker, struct worker);
	wrk_task(w, (fftask*)pt, func, param);
}

static phi_kevent* core_kev_alloc(uint worker)
{
	PHI_ASSERT(worker < cc->wx.workers.len);
	struct worker *w = ffslice_itemT(&cc->wx.workers, worker, struct worker);
	phi_kevent *kev = (void*)zzkq_kev_alloc(&w->kq);
	if (kev) {
		((struct zzkevent*)kev)->kcall.q = &w->kq_kcq.kcq;
		dbglog("kev alloc: %p", kev);
	}
	return kev;
}

static void core_kev_free(uint worker, phi_kevent *kev)
{
	if (!kev) return;

	PHI_ASSERT(worker < cc->wx.workers.len);
	struct worker *w = ffslice_itemT(&cc->wx.workers, worker, struct worker);
	ffkcall_cancel(&((struct zzkevent*)kev)->kcall);
	zzkq_kev_free(&w->kq, (void*)kev);
	dbglog("kev free: %p", kev);
}

static int core_kq_attach(uint worker, phi_kevent *kev, fffd fd, uint flags)
{
	PHI_ASSERT(worker < cc->wx.workers.len);
	struct worker *w = ffslice_itemT(&cc->wx.workers, worker, struct worker);
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
	struct phi_woeh_task *wt = param;
	fftask *t = (fftask*)&wt->task;
	core_task(wt->worker, &wt->task, t->handler, t->param);
}

static int core_woeh(uint worker, fffd fd, struct phi_woeh_task *wt, phi_task_func func, void *param, uint flags)
{
	if (!cc->woeh_obj
		&& !(cc->woeh_obj = woeh_create())) {
		syserrlog("woeh create");
		return -1;
	}

	fftask *t = (fftask*)&wt->task;
	t->handler = func;
	t->param = param;
	wt->worker = worker;
	return woeh_add(cc->woeh_obj, fd, woeh_task, wt, flags);
}

#endif

static int core_workers_available()
{
	return wrkx_available(&cc->wx);
}

static uint core_worker_assign(uint flags)
{
	return wrkx_assign(&cc->wx, flags);
}

static void core_worker_release(uint worker)
{
	PHI_ASSERT(worker < cc->wx.workers.len);
	wrkx_release(&cc->wx, worker);
}

static int core_wrk_creating(uint iw)
{
	if (cc->kcq_lazy_start)
		return zzkcq_start(&cc->kcq, iw);
	return 0;
}


FF_EXPORT void phi_core_destroy()
{
	if (cc == NULL) return;

	dbglog("stats: %L modules; %L workers"
		, cc->mods.len, cc->wx.workers.len);

#ifdef FF_WIN
	woeh_free(cc->woeh_obj);
#endif

	wrkx_stop(&cc->wx);
	zzkcq_destroy(&cc->kcq);
	wrkx_wait_stopped(&cc->wx);

	struct core_mod *m;
	FFSLICE_WALK(&cc->mods, m) {
		mod_destroy(m);
	}
	ffvec_free(&cc->mods);

	tracks_destroy();
	qm_destroy();
	wrkx_destroy(&cc->wx);
	win_sleep_destroy();
	ffmem_free(cc);  cc = NULL;
}

static void core_logv(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, va_list va)
{}
static void core_log(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, ...)
{}

/** Normalize workers number */
static uint wrk_n(uint n)
{
	if (n == 0)
		return 1;
	if (n == ~0U) {
		ffsysconf sc;
		ffsysconf_init(&sc);
		return ffsysconf_get(&sc, FFSYSCONF_NPROCESSORS_ONLN);
	}
	return ffmin(n, 100);
}

FF_EXPORT phi_core* phi_core_create(struct phi_core_conf *conf)
{
	FF_ASSERT(sizeof(phi_task) >= sizeof(fftask));
	FF_ASSERT(sizeof(phi_timer) >= sizeof(fftimerqueue_node));

	if (cc) return NULL;

	core = &_core;
	core->conf = *conf;
	if (core->conf.log == NULL) {
		core->conf.log = core_log;
		core->conf.logv = core_logv;
	}
	if (!core->conf.code_page) core->conf.code_page = FFUNICODE_WIN1252;
	if (!core->conf.timer_interval_msec) core->conf.timer_interval_msec = 100;

	cc = ffmem_new(struct core_ctx);
	ffvec_allocT(&cc->mods, 8, struct core_mod);
	fftime_local(&cc->tz);
	tracks_init();
	qm_init();
	win_sleep_init();

	core->conf.workers = wrk_n(core->conf.workers);
	if (core->conf.io_workers == ~0U) {
		core->conf.io_workers = core->conf.workers;
		cc->kcq_lazy_start = 1;
	}
	if (core->conf.max_tasks == 0)
		core->conf.max_tasks = 100;

	if (core->conf.io_workers
		&& zzkcq_create(&cc->kcq, core->conf.io_workers, core->conf.max_tasks, 0))
		goto err;

	struct wrk_conf wc = {
		.max_tasks = core->conf.max_tasks,

		.kcq_sq = cc->kcq.sq,
		.kcq_sq_sem = cc->kcq.sem,
	};
	if (!!wrkx_init(&cc->wx, core->conf.workers, &wc)) {
		goto err;
	}

	return core;

err:
	phi_core_destroy();
	return NULL;
}

FF_EXPORT void phi_core_run()
{
	int ikcq = (cc->kcq_lazy_start) ? 0 : -1;
	if (zzkcq_start(&cc->kcq, ikcq))
		return;

	wrkx_run(&cc->wx);
}

extern phi_track_if phi_track_iface;
static phi_core _core = {
	.version_str = PHI_VERSION_STR,
	.track = &phi_track_iface,
	.time = core_time,
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
	.workers_available = core_workers_available,
	.worker_assign = core_worker_assign,
	.worker_release = core_worker_release,
};
