/** KQ timer
2023, Simon Zolin */

/*
zzkq_timer_init
zzkq_timer_create zzkq_timer_destroy
zzkq_timer_start zzkq_timer_stop
zzkq_timer_active
*/

#pragma once
#include <ffsys/timer.h>

struct zzkq_timer {
	fftimer timer;
	struct zzkevent kev;
};

static inline void zzkq_timer_init(struct zzkq_timer *kt)
{
	kt->timer = FFTIMER_NULL;
}

static inline void zzkq_timer_destroy(struct zzkq_timer *kt, ffkq kq)
{
	fftimer_close(kt->timer, kq);  kt->timer = FFTIMER_NULL;
}

static inline int zzkq_timer_create(struct zzkq_timer *kt)
{
	if (FFTIMER_NULL == (kt->timer = fftimer_create(0)))
		return -1;
	return 0;
}

static inline int zzkq_timer_start(struct zzkq_timer *kt, ffkq kq, ffuint interval_msec, zzkevent_func func, void *param)
{
	kt->kev.rhandler = func;
	kt->kev.obj = param;
	kt->kev.rtask.active = 1;
	if (0 != fftimer_start(kt->timer, kq, &kt->kev, interval_msec))
		return -1;
	return 0;
}

static inline int zzkq_timer_stop(struct zzkq_timer *kt, ffkq kq)
{
	if (!!fftimer_stop(kt->timer, kq))
		return -1;
	kt->kev.rtask.active = 0;
	return 0;
}

static inline int zzkq_timer_active(struct zzkq_timer *kt)
{
	return kt->kev.rtask.active;
}
