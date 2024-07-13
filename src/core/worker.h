/** phiola: Core: worker threads
2023, Simon Zolin */

#define ZZKQ_LOG_SYSERR  (PHI_LOG_ERR | PHI_LOG_SYS)
#define ZZKQ_LOG_ERR  PHI_LOG_ERR
#define ZZKQ_LOG_DEBUG  PHI_LOG_DEBUG
#define ZZKQ_LOG_EXTRA  PHI_LOG_EXTRA

#define FFTASKQUEUE_LOG_DEBUG  PHI_LOG_DEBUG
#define FFTASKQUEUE_LOG_EXTRA  PHI_LOG_EXTRA

#include <util/kq.h>
#include <util/kq-kcq.h>
#include <util/kq-tq.h>
#include <util/kq-timer.h>
#include <ffsys/sysconf.h>
#include <ffsys/thread.h>
#include <ffsys/perf.h>
#include <ffsys/timerqueue.h>
#include <ffbase/vector.h>

struct wrk_conf {
	uint			max_tasks;
	ffringqueue*	kcq_sq;
	ffsem			kcq_sq_sem;
};

struct wrk_ctx {
	struct wrk_conf conf;
	ffvec			workers; // struct worker[]
	uint			n_reserved;
};

struct worker {
	uint			initialized;
	ffthread		th;
	uint64			tid;
	struct zzkq		kq;
	ffatomic		njobs;

	struct zzkq_kcq kq_kcq;

	fftaskqueue		tq;
	struct zzkq_tq	kq_tq;

	fftimerqueue		timerq;
	struct zzkq_timer	kq_timer;
	fftime				now;
	uint				now_msec;
	fftimerqueue_node	tmr_stop;
};

void wrk_task(struct worker *w, fftask *t, phi_task_func func, void *param)
{
	if (func == NULL) {
		fftaskqueue_del(&w->tq, t);
		dbglog("task removed: %p", t);
		return;
	}

	t->handler = func;
	t->param = param;
	dbglog("task add: %p %p %p", t, t->handler, t->param);
	zzkq_tq_post(&w->kq_tq, t);
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

void wrk_timer(struct worker *w, fftimerqueue_node *t, int interval_msec, phi_task_func func, void *param)
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

static void wrk_affinity(ffthread th, uint i, uint mask)
{
	if (!mask) return;

	uint cpu;
	for (uint j = 0;  ;  j++) {
		cpu = ffbit_rfind32(mask);
		if (!cpu)
			return;
		cpu--;
		if (j == i)
			break;
		mask &= ~(1U << cpu);
	}

	ffthread_cpumask cm = {};
	ffthread_cpumask_set(&cm, cpu);
	if (ffthread_affinity(th, &cm)) {
		syserrlog("set CPU affinity");
		return;
	}
	dbglog("worker #%u CPU affinity: %u", i, cpu);
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

static int wrk_create(struct worker *w, const struct wrk_conf *conf)
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

		.max_objects = conf->max_tasks,
		.events_wait = 64,
	};
	if (!!zzkq_create(&w->kq, &kc))
		return -1;

	if (conf->kcq_sq
		&& zzkqkcq_connect(&w->kq_kcq, w->kq.kq, conf->max_tasks, conf->kcq_sq, conf->kcq_sq_sem))
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
	zzkqkcq_disconnect(&w->kq_kcq, w->kq.kq);
	zzkq_tq_detach(&w->kq_tq, w->kq.kq);
	zzkq_timer_destroy(&w->kq_timer, w->kq.kq);
	zzkq_destroy(&w->kq);
}

int wrkx_init(struct wrk_ctx *wx, uint n, struct wrk_conf *conf)
{
	ffvec_zallocT(&wx->workers, n, struct worker);

	struct worker *w = ffslice_pushT(&wx->workers, wx->workers.cap, struct worker);
	if (!!wrk_create(w, conf))
		return -1;
	wx->n_reserved = 1;
	wx->conf = *conf;
	return 0;
}

void wrkx_run(struct wrk_ctx *wx)
{
	struct worker *w = ffslice_itemT(&wx->workers, 0, struct worker);
	if (core->conf.run_detach)
		wrk_start(w);
	wrk_affinity(ffthread_current(), 0, core->conf.cpu_affinity);
	if (!core->conf.run_detach)
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
	if (FFINT_READONCE(wx->workers.len) < wx->workers.cap)
		return 1;

	struct worker *w;
	FFSLICE_WALK(&wx->workers, w) {
		if (0 == ffatomic_load(&w->njobs)) {
			if (w == wx->workers.ptr && wx->workers.len > 1)
				continue; // main worker must be free to manage the queue
			dbglog("worker #%u is available"
				, (int)(w - (struct worker*)wx->workers.ptr));
			return 1;
		}
	}
	return 0;
}

static void wrk_thread_name(ffthread th, uint i)
{
#ifdef FF_LINUX
	char name[8] = "wk";
	name[2] = i / 10 + '0';
	name[3] = i % 10 + '0';
	pthread_setname_np(th, name);
#endif
}

/** Find the least busy worker; create thread if not initialized.
Note: the algorithm doesn't really guarantee that the least busy worker is selected,
 but currently only the main thread creates conversion tracks, so it's not a real problem.
Return worker ID */
uint wrkx_assign(struct wrk_ctx *wx, uint flags)
{
	struct worker *w = NULL, *it;
	unsigned i_reserved = 0, failed = 0, wid, nj;

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

	// The selected worker `w` was the least busy one recently.

	if (jobs_min != 0 && wx->workers.len < wx->workers.cap) {
		// no free workers; activate one from pool
		i_reserved = ffint_fetch_add(&wx->n_reserved, 1);
		if (i_reserved >= wx->workers.cap) {
			i_reserved = 0;
		} else {
			struct worker *wn = (struct worker*)wx->workers.ptr + i_reserved;
			failed = (core_wrk_creating(i_reserved)
				|| wrk_create(wn, &wx->conf)
				|| wrk_start(wn));
			if (ff_unlikely(failed)) {
				wrk_destroy(wn);
				ffatomic_store(&wn->njobs, ~0U);
				// Note: this thread slot will never be used.
				// And it's futile to do anything clever when the system is failing.
			} else {
				w = wn;
			}
		}
	}

done:
	wid = w - (struct worker*)wx->workers.ptr;
	nj = ffatomic_fetch_add(&w->njobs, 1) + 1;

	if (i_reserved) {
		// Wait for others to complete the preparation of the worker slots reserved before us
		ffintz_wait_until_equal(&wx->workers.len, i_reserved);

		ffcpu_fence_release(); // the data is written before the counter
		wx->workers.len = i_reserved + 1;

		if (!failed) {
			wrk_thread_name(w->th, wid);
			wrk_affinity(w->th, wid, core->conf.cpu_affinity);
		}
	}

	dbglog("worker #%u assign: jobs:%u", wid, nj);
	return wid;
}

static void wrkx_release(struct wrk_ctx *wx, uint wid)
{
	struct worker *w = ffslice_itemT(&wx->workers, wid, struct worker);
	int nj = ffatomic_fetch_add(&w->njobs, -1) - 1;
	FF_ASSERT(nj >= 0);
	dbglog("worker #%u release: jobs:%u", wid, nj);
}
