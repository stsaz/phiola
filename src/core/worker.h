/** phiola: Core: worker threads
2023, Simon Zolin */

#define ZZKQ_LOG_SYSERR  (PHI_LOG_ERR | PHI_LOG_SYS)
#define ZZKQ_LOG_ERR  PHI_LOG_ERR
#define ZZKQ_LOG_DEBUG  PHI_LOG_DEBUG
#define ZZKQ_LOG_EXTRA  PHI_LOG_EXTRA

#include <util/kq.h>
#include <util/kq-tq.h>
#include <util/kq-timer.h>
#include <ffsys/sysconf.h>
#include <ffsys/thread.h>
#include <ffsys/perf.h>
#include <ffsys/timerqueue.h>
#include <ffbase/vector.h>

struct wrk_ctx {
	ffvec workers; // struct worker[]
	fflock lock;
};

struct worker {
	uint initialized;
	ffthread th;
	uint64 tid;
	struct zzkq kq;
	ffatomic njobs;

	fftaskqueue tq;
	struct zzkq_tq kq_tq;

	fftimerqueue timerq;
	struct zzkq_timer kq_timer;
	fftime now;
	uint now_msec;
	fftimerqueue_node tmr_stop;
};

void wrk_task(struct worker *w, phi_task *t, phi_task_func func, void *param)
{
	if (func == NULL) {
		fftaskqueue_del(&w->tq, (fftask*)t);
		dbglog("task removed: %p", t);
		return;
	}

	t->handler = func;
	t->param = param;
	dbglog("task add: %p %p %p", t, t->handler, t->param);
	zzkq_tq_post(&w->kq_tq, (fftask*)t);
}

static void timer_suspend(void *param)
{
	struct worker *w = param;
	if (!!zzkq_timer_stop(&w->kq_timer, w->kq.kq))
		syserrlog("timer stop");
	dbglog("ktimer: stopped");
}

static void on_timer(void *param)
{
	struct worker *w = param;
	fftimer_consume(w->kq_timer.timer);
	w->now = fftime_monotonic();
	w->now_msec = fftime_to_msec(&w->now);
	uint n = fftimerqueue_process(&w->timerq, w->now_msec);
	extralog("processed %u timers", n);
	if (n != 0 && w->timerq.tree.len == 0) {
		fftimerqueue_add(&w->timerq, &w->tmr_stop, w->now_msec, -10000, timer_suspend, w);
	}
}

void wrk_timer(struct worker *w, phi_timer *t, int interval_msec, phi_task_func func, void *param)
{
	if (interval_msec == 0) {
		dbglog("timer:%p stop", t);
		fftimerqueue_remove(&w->timerq, t);
		if (w->timerq.tree.len == 0) {
			fftimerqueue_add(&w->timerq, &w->tmr_stop, w->now_msec, -10000, timer_suspend, w);
		}
		return;
	}

	fftimerqueue_remove(&w->timerq, &w->tmr_stop);

	if (!zzkq_timer_active(&w->kq_timer)) {
		if (!!zzkq_timer_start(&w->kq_timer, w->kq.kq, core->conf.timer_interval_msec, on_timer, w)) {
			syserrlog("timer start");
			return;
		}
		dbglog("ktimer: %umsec", core->conf.timer_interval_msec);
	}

	dbglog("timer:%p  interval:%d  handler:%p  param:%p"
		, t, interval_msec, func, param);

	fftimerqueue_add(&w->timerq, t, w->now_msec, interval_msec, func, param);
}

static int FFTHREAD_PROCCALL wrk_run(void *param)
{
	struct worker *w = param;
	w->tid = ffthread_curid();
	return zzkq_run(&w->kq);
}

static int wrk_start(struct worker *w)
{
	if (FFTHREAD_NULL == (w->th = ffthread_create(wrk_run, w, 0))) {
		syserrlog("ffthread_create");
		return -1;
	}
	return 0;
}

static int wrk_create(struct worker *w)
{
	w->th = FFTHREAD_NULL;
	zzkq_init(&w->kq);
	fftimerqueue_init(&w->timerq);
	zzkq_timer_init(&w->kq_timer);

	struct zzkq_conf kc = {
		.log = {
			.level = core->conf.log_level,
			.func = core->conf.log,
			.obj = core->conf.log_obj,
			.ctx = "core",
		},

		.max_objects = core->conf.max_tasks,
		.events_wait = 64,
	};
	if (!!zzkq_create(&w->kq, &kc))
		return -1;

	fftaskqueue_init(&w->tq);
	w->tq.log.level = core->conf.log_level;
	w->tq.log.func = core->conf.log;
	w->tq.log.obj = core->conf.log_obj;
	w->tq.log.ctx = "core";
	if (!!zzkq_tq_attach(&w->kq_tq, w->kq.kq, &w->tq)) {
		syserrlog("zzkq_tq_attach");
		return -1;
	}

	if (!!zzkq_timer_create(&w->kq_timer)) {
		syserrlog("timer create");
		return -1;
	}

	w->initialized = 1;
	on_timer(w);
	return 0;
}

static void wrk_destroy(struct worker *w)
{
	if (w->th != FFTHREAD_NULL) {
		zzkq_stop(&w->kq);
		ffthread_join(w->th, -1, NULL);  w->th = FFTHREAD_NULL;
		dbglog("thread exited");
	}
	zzkq_tq_detach(&w->kq_tq, w->kq.kq);
	zzkq_timer_destroy(&w->kq_timer, w->kq.kq);
	zzkq_destroy(&w->kq);
}

/** Normalize workers number */
static uint wrk_n(uint n)
{
	if (n == 0) {
		n = 1;
	} else if (n == ~0U) {
		ffsysconf sc;
		ffsysconf_init(&sc);
		n = ffsysconf_get(&sc, FFSYSCONF_NPROCESSORS_ONLN);
	}
	return ffmin(n, 100);
}

int wrkx_init(struct wrk_ctx *wx)
{
	core->conf.workers = wrk_n(core->conf.workers);
	if (core->conf.max_tasks == 0)
		core->conf.max_tasks = 100;

	ffvec_zallocT(&wx->workers, core->conf.workers, struct worker);

	struct worker *w = ffslice_pushT(&wx->workers, wx->workers.cap, struct worker);
	if (!!wrk_create(w))
		return -1;
	return 0;
}

void wrkx_run(struct wrk_ctx *wx)
{
	struct worker *w = ffslice_itemT(&wx->workers, 0, struct worker);
	if (core->conf.run_detach)
		wrk_start(w);
	else
		wrk_run(w);
}

void wrkx_stop(struct wrk_ctx *wx)
{
	struct worker *w;
	FFSLICE_WALK(&wx->workers, w) {
		zzkq_stop(&w->kq);
	}
}

void wrkx_wait_stopped(struct wrk_ctx *wx)
{
	struct worker *w;
	FFSLICE_WALK(&wx->workers, w) {
		wrk_destroy(w);
	}
}

void wrkx_destroy(struct wrk_ctx *wx)
{
	ffvec_free(&wx->workers);
}

static int wrkx_available(struct wrk_ctx *wx)
{
	if (wx->workers.len < wx->workers.cap)
		return 1;

	struct worker *w;
	FFSLICE_WALK(&wx->workers, w) {
		if (0 == ffatomic_load(&w->njobs)) {
			if (w == wx->workers.ptr && wx->workers.len > 1)
				continue; // main worker must be free to manage the queue
			return 1;
		}
	}
	return 0;
}

/** Find the least busy worker; create thread if not initialized.
Return worker ID */
uint wrkx_assign(struct wrk_ctx *wx, uint flags)
{
	struct worker *w = NULL, *it;

	if (flags == 0) {
		w = wx->workers.ptr;
		goto done;
	}

	uint jobs_min = ~0U;
	FFSLICE_WALK(&wx->workers, it) {
		uint nj = ffatomic_load(&it->njobs);
		if (nj < jobs_min) {
			if (it == wx->workers.ptr && wx->workers.len > 1)
				continue; // main worker must be free to manage the queue
			jobs_min = nj;
			w = it;
			if (nj == 0)
				break;
		}
	}

	// The selected worker `w` is the least busy one.

	if (jobs_min != 0 && wx->workers.len < wx->workers.cap) {
		// no free workers; activate one from pool
		fflock_lock(&wx->lock);
		if (wx->workers.len < wx->workers.cap) {
			w = &((struct worker*)wx->workers.ptr)[wx->workers.len];
			int failed = (wrk_create(w) || wrk_start(w));
			if (ff_unlikely(failed)) {
				wrk_destroy(w);
				w = wx->workers.ptr; // use main worker
			} else {
				ffcpu_fence_release(); // the data is written before the counter
				wx->workers.len++;
			}
		}
		fflock_unlock(&wx->lock);
	}

done:
	{
	uint wid = w - (struct worker*)wx->workers.ptr;
	uint nj = ffatomic_fetch_add(&w->njobs, 1) + 1;
	dbglog("worker #%u assign: jobs:%u", wid, nj);
	return wid;
	}
}

static void wrkx_release(struct wrk_ctx *wx, uint wid)
{
	struct worker *w = ffslice_itemT(&wx->workers, wid, struct worker);
	int nj = ffatomic_fetch_add(&w->njobs, -1) - 1;
	FF_ASSERT(nj >= 0);
	dbglog("worker #%u release: jobs:%u", wid, nj);
}
