/** Bridge between KQ and KCQ

Workflow:

	[KQ]       [KCQ]
	Submit --> SQ
	           Exec()
	CQ <------ Completed
	*  <------ Signal
	handler()

* A KQ job submits some work for KCQ.SQ via zzkevent
* KCQ performs the operation and submits the result to KQ.CQ,
   then it signals KQ
* KQ job processes events from CQ and calls a completion handler

2023, Simon Zolin */

#pragma once
#include "kq.h"
#include "kcq.h"

struct zzkq_kcq {
	struct ffkcallqueue kcq;
	struct zzkevent kev;
	ffuint polling_mode;
};

static inline void zzkqkcq_init(struct zzkq_kcq *kk)
{
	kk->kcq.kqpost = FFKQ_NULL;
}

static inline void zzkqkcq_disconnect(struct zzkq_kcq *kk, ffkq kq)
{
	ffkq_post_detach(kk->kcq.kqpost, kq);  kk->kcq.kqpost = FFKQ_NULL;
	ffrq_free(kk->kcq.cq);  kk->kcq.cq = NULL;
}

static void _zzkqkcq_onsignal(struct zzkq_kcq *kk)
{
	ffkq_post_consume(kk->kcq.kqpost);
	ffkcallq_process_cq(kk->kcq.cq);
	if (kk->polling_mode)
		ffkq_post(kk->kcq.kqpost, &kk->kev);
}

static inline int zzkqkcq_connect(struct zzkq_kcq *kk, ffkq kq, ffuint max_cq_jobs, ffringqueue *sq, ffsem sq_sem)
{
	kk->kcq.sq = sq;
	kk->kcq.sem = sq_sem;
	kk->polling_mode = !!(sq_sem == FFSEM_NULL);

	if (NULL == (kk->kcq.cq = ffrq_alloc(max_cq_jobs))) {
		goto err;
	}

	kk->kev.rhandler = (zzkevent_func)_zzkqkcq_onsignal;
	kk->kev.obj = kk;
	kk->kev.rtask.active = 1;
	if (FFKQ_NULL == (kk->kcq.kqpost = ffkq_post_attach(kq, &kk->kev))) {
		goto err;
	}
	kk->kcq.kqpost_data = &kk->kev;
	return 0;

err:
	zzkqkcq_disconnect(kk, kq);
	return -1;
}

static inline void zzkqkcq_kev_attach(struct zzkq_kcq *kk, struct zzkevent *kev)
{
	kev->kcall.q = &kk->kcq;
}
