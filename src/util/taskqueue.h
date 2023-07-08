/** ff: task queue: First in, first out.  One reader/deleter, multiple writers.
2013, 2022, Simon Zolin
*/

/*
fftask_set
fftaskqueue_init
fftaskqueue_active
fftaskqueue_post fftaskqueue_post4
fftaskqueue_del
fftaskqueue_run
*/

#pragma once
#include <ffbase/list.h>
#include <ffbase/lock.h>

typedef void (*fftask_handler)(void *param);

typedef struct fftask {
	fftask_handler handler;
	void *param;
	ffchain_item sib;
} fftask;

#define fftask_set(t, func, udata) \
	(t)->handler = (func),  (t)->param = (udata)

typedef struct fftaskqueue {
	fflist tasks; //fftask[]
	fflock lk;
} fftaskqueue;

static inline void fftaskqueue_init(fftaskqueue *tq)
{
	fflist_init(&tq->tasks);
	fflock_init(&tq->lk);
}

/** Return TRUE if a task is in the queue. */
#define fftaskqueue_active(tq, t)  ((t)->sib.next != NULL)

/** Add item into task queue.  Thread-safe.
Return 1 if the queue was empty. */
static inline ffuint fftaskqueue_post(fftaskqueue *tq, fftask *t)
{
	ffuint r = 0;

	fflock_lock(&tq->lk);
	if (fftaskqueue_active(tq, t))
		goto done;
	r = fflist_empty(&tq->tasks);
	fflist_add(&tq->tasks, &t->sib);

done:
	fflock_unlock(&tq->lk);
	return r;
}

#define fftaskqueue_post4(tq, t, func, _param) \
do { \
	(t)->handler = func; \
	(t)->param = _param; \
	fftaskqueue_post(tq, t); \
} while (0)

/** Remove item from task queue. */
static inline void fftaskqueue_del(fftaskqueue *tq, fftask *t)
{
	fflock_lock(&tq->lk);
	if (!fftaskqueue_active(tq, t))
		goto done;
	fflist_rm(&tq->tasks, &t->sib);

done:
	fflock_unlock(&tq->lk);
}

/** Call a handler for each task.
Return the number of tasks executed. */
static inline ffuint fftaskqueue_run(fftaskqueue *tq)
{
	ffchain_item *it, *sentl = fflist_sentl(&tq->tasks);
	ffuint n = 0;

	for (;;) {

		it = FFINT_READONCE(fflist_first(&tq->tasks));
		if (it == sentl)
			break; // list is empty

		fflock_lock(&tq->lk);
		fflist_rm(&tq->tasks, it);
		fflock_unlock(&tq->lk);

		fftask *t = FF_STRUCTPTR(fftask, sib, it);
		t->handler(t->param);

		n++;
	}

	return n;
}
