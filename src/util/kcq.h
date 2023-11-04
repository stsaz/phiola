/** Process events from kernel call queue; multiple workers
2022, Simon Zolin */

/*
zzkcq_create zzkcq_destroy
zzkcq_start
*/

#pragma once
#include <ffsys/kcall.h>
#include <ffsys/semaphore.h>
#include <ffsys/thread.h>
#include <ffbase/vector.h>
#include <ffbase/ringqueue.h>

struct zzkcq {
	ffringqueue *sq;
	ffsem sem;
	ffvec workers; // ffthread[]
	ffuint stop;
	ffuint polling_mode;
};

static inline int zzkcq_create(struct zzkcq *k, ffuint workers, ffuint max_jobs, ffuint polling_mode)
{
	if (NULL == (k->sq = ffrq_alloc(max_jobs))) {
		return -1;
	}

	if (!polling_mode
		&& FFSEM_NULL == (k->sem = ffsem_open(NULL, 0, 0))) {
		goto err;
	}

	if (NULL == ffvec_allocT(&k->workers, workers, ffthread))
		goto err;

	k->workers.len = workers;
	k->polling_mode = polling_mode;
	return 0;

err:
	ffrq_free(k->sq);
	if (k->sem != FFSEM_NULL)
		ffsem_close(k->sem);
	return -1;
}

static inline void zzkcq_destroy(struct zzkcq *k)
{
	FFINT_WRITEONCE(k->stop, 1);
	ffthread *it;

	if (k->sem != FFSEM_NULL) {
		// dbglog("stopping kcall workers");
		FFSLICE_WALK(&k->workers, it) {
			ffsem_post(k->sem);
		}
	}

	FFSLICE_WALK(&k->workers, it) {
		if (*it != FFTHREAD_NULL)
			ffthread_join(*it, -1, NULL);
	}

	if (k->sem != FFSEM_NULL)
		ffsem_close(k->sem);
	ffrq_free(k->sq);
	ffvec_free(&k->workers);
}

static int FFTHREAD_PROCCALL _zzkcq_worker(void *param)
{
	struct zzkcq *k = param;
	// dbglog("entering kcall loop");
	while (!FFINT_READONCE(k->stop)) {
		ffkcallq_process_sq(k->sq);
		if (!k->polling_mode)
			ffsem_wait(k->sem, -1);
	}
	// dbglog("left kcall loop");
	return 0;
}

static inline int zzkcq_start(struct zzkcq *k)
{
	ffthread *it;
	FFSLICE_WALK(&k->workers, it) {
		if (FFTHREAD_NULL == (*it = ffthread_create(_zzkcq_worker, k, 0))) {
			// syserrlog("thread create");
			return -1;
		}
	}
	return 0;
}
