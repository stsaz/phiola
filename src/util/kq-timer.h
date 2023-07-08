/** KQ timer
2023, Simon Zolin */

#pragma once
#include <FFOS/timer.h>

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

static inline int zzkq_timer_create(struct zzkq_timer *kt, ffkq kq, ffuint interval_msec, zzkevent_func func, void *param)
{
	if (FFTIMER_NULL == (kt->timer = fftimer_create(0)))
		return -1;
	kt->kev.rhandler = func;
	kt->kev.obj = param;
	kt->kev.rtask.active = 1;
	if (0 != fftimer_start(kt->timer, kq, &kt->kev, interval_msec))
		return -1;
	return 0;
}
