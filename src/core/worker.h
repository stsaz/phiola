/** phiola: Core: worker threads
2023, Simon Zolin */

#define ZZKQ_LOG_SYSERR  (PHI_LOG_ERR | PHI_LOG_SYS)
#define ZZKQ_LOG_ERR  PHI_LOG_ERR
#define ZZKQ_LOG_DEBUG  PHI_LOG_DEBUG
#define ZZKQ_LOG_EXTRA  PHI_LOG_EXTRA

#include <util/kq.h>
#include <util/kq-tq.h>
#include <util/kq-timer.h>
#include <FFOS/sysconf.h>
#include <FFOS/thread.h>
#include <FFOS/perf.h>
#include <FFOS/timerqueue.h>
#include <ffbase/vector.h>

struct wrk_ctx {
	ffvec workers; // struct worker[]
};

struct worker {
	ffthread th;
	uint64 tid;
	struct zzkq kq;

	fftaskqueue tq;
	struct zzkq_tq kq_tq;

	fftimerqueue timerq;
	struct zzkq_timer kq_timer;
	fftime now;
	uint now_msec;
	fftimerqueue_node tmr_stop;
};

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

static void wrk_timer(struct worker *w, phi_timer *t, int interval_msec, phi_task_func func, void *param)
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
		.log_level = core->conf.log_level,
		.log = core->conf.log,
		.log_obj = core->conf.log_obj,
		.log_ctx = "core",

		.max_objects = core->conf.max_tasks,
		.events_wait = 64,
	};
	if (!!zzkq_create(&w->kq, &kc))
		return -1;

	fftaskqueue_init(&w->tq);
	if (!!zzkq_tq_attach(&w->kq_tq, w->kq.kq, &w->tq)) {
		syserrlog("zzkq_tq_attach");
		return -1;
	}

	if (!!zzkq_timer_create(&w->kq_timer)) {
		syserrlog("timer create");
		return -1;
	}

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

int wrkx_init(struct wrk_ctx *wx)
{
	if (core->conf.max_tasks == 0)
		core->conf.max_tasks = 100;

	ffvec_zallocT(&wx->workers, 1, struct worker);

	struct worker *w = ffslice_pushT(&wx->workers, wx->workers.cap, struct worker);
	if (!!wrk_create(w))
		return -1;
	return 0;
}

void wrkx_stop(struct wrk_ctx *wx)
{
	struct worker *w;
	FFSLICE_WALK(&wx->workers, w) {
		zzkq_stop(&w->kq);
	}
}

void wrkx_destroy(struct wrk_ctx *wx)
{
	struct worker *w;
	FFSLICE_WALK(&wx->workers, w) {
		wrk_destroy(w);
	}
	ffvec_free(&wx->workers);
}
