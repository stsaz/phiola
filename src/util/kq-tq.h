/** Bridge between KQ and TQ
2023, Simon Zolin */

#pragma once
#include "kq.h"
#include "taskqueue.h"

struct zzkq_tq {
	fftaskqueue *tq;
	ffkq_postevent kqpost;
	struct zzkevent kev;
};

static inline void zzkq_tq_detach(struct zzkq_tq *kt, ffkq kq)
{
	ffkq_post_detach(kt->kqpost, kq);  kt->kqpost = FFKQ_NULL;
}

static void zzkq_tq_process(struct zzkq_tq *kt)
{
	ffkq_post_consume(kt->kqpost);
	fftaskqueue_run(kt->tq);
}

/** Attach TQ processor to KQ */
static inline int zzkq_tq_attach(struct zzkq_tq *kt, ffkq kq, fftaskqueue *tq)
{
	kt->kev.rhandler = (void*)zzkq_tq_process;
	kt->kev.obj = kt;
	kt->kev.rtask.active = 1;
	if (FFKQ_NULL == (kt->kqpost = ffkq_post_attach(kq, &kt->kev)))
		return -1;
	kt->tq = tq;
	return 0;
}

/** Add a task to TQ and signal KQ */
static inline int zzkq_tq_post(struct zzkq_tq *kt, fftask *tsk)
{
	if (1 == fftaskqueue_post(kt->tq, tsk))
		return ffkq_post(kt->kqpost, &kt->kev);
	return 0;
}
