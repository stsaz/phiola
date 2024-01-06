/** Windows object event handler - call a user function when a kernel object signals.
2015, Simon Zolin
*/

/*
woeh_create woeh_free
woeh_add
woeh_rm
*/

/*
       (HANDLE)
KERNEL -------> WOEH-thread -> user_function()
*/

#pragma once
#include <ffsys/thread.h>
#include <ffsys/error.h>
#include <ffbase/lock.h>
#include <ffbase/atomic.h>

typedef void (*woeh_handler_t)(void *udata);

struct woeh_item {
	woeh_handler_t handler;
	void *udata; // bit[0]: one-shot
};

typedef struct woeh {
	uint count;
	HANDLE hdls[MAXIMUM_WAIT_OBJECTS]; //hdls[0] is wake_evt
	struct woeh_item items[MAXIMUM_WAIT_OBJECTS];

	HANDLE wake_evt, wait_evt;
	ffthread thd;
	uint tid;
	uint thderr;
	uint cmd; // enum WOEH_CMD
	fflock lk;
} woeh;

enum WOEH_CMD {
	WOEH_CMD_RUN,
	WOEH_CMD_QUIT,
	WOEH_CMD_WAIT,
};

static inline void woeh_free(woeh *oh)
{
	if (!oh) return;

	if (oh->wake_evt != NULL) {
		FFINT_WRITEONCE(oh->cmd, WOEH_CMD_QUIT);
		SetEvent(oh->wake_evt);
	}

	if (oh->thd != FFTHREAD_NULL)
		ffthread_join(oh->thd, (uint)-1, NULL);

	if (oh->wake_evt != NULL)
		CloseHandle(oh->wake_evt);
	if (oh->wait_evt != NULL)
		CloseHandle(oh->wait_evt);

	ffmem_free(oh);
}

static void _woeh_rm(woeh *oh, uint i)
{
	oh->count--;
	if (i != oh->count) {
		//move the last element into a hole
		ffmem_move(oh->items + i, oh->items + oh->count, sizeof(struct woeh_item));
		ffmem_move(oh->hdls + i, oh->hdls + oh->count, sizeof(HANDLE));
	}
}

static int FFTHREAD_PROCCALL woeh_evt_handler(void *param)
{
	woeh *oh = param;
	oh->tid = ffthread_curid();

	for (;;) {
		uint count = oh->count; //oh->count may be incremented
		DWORD i = WaitForMultipleObjects(count, oh->hdls, 0, INFINITE);

		if (i >= WAIT_OBJECT_0 + count) {
			fflock_lock(&oh->lk);
			oh->count = (uint)-1;
			oh->thderr = (i == WAIT_FAILED) ? fferr_last() : 0;
			fflock_unlock(&oh->lk);
			return 1;
		}

		if (i == 0) {
			//wake_evt has signaled
			ssize_t cmd = FFINT_READONCE(oh->cmd);
			if (cmd == WOEH_CMD_QUIT) {
				break;

			} else if (cmd == WOEH_CMD_WAIT) {
				SetEvent(oh->wait_evt);
				fflock_lock(&oh->lk); //wait until the arrays are modified
				oh->cmd = WOEH_CMD_RUN;
				fflock_unlock(&oh->lk);
			}

			continue;
		}

		struct woeh_item h = oh->items[i];

		if ((size_t)h.udata & 1) {
			h.udata = (void*)((size_t)h.udata & ~1);
			fflock_lock(&oh->lk);
			_woeh_rm(oh, i);
			fflock_unlock(&oh->lk);
		}

		h.handler(h.udata);
	}

	return 0;
}

static inline woeh* woeh_create()
{
	woeh *oh = ffmem_new(woeh);
	if (oh == NULL) return NULL;

	oh->wake_evt = CreateEvent(NULL, 0, 0, NULL);
	oh->wait_evt = CreateEvent(NULL, 0, 0, NULL);
	if (oh->wake_evt == NULL || oh->wait_evt == NULL)
		goto fail;

	oh->hdls[oh->count++] = oh->wake_evt;

	if (FFTHREAD_NULL == (oh->thd = ffthread_create(&woeh_evt_handler, oh, 0)))
		goto fail;

	fflock_init(&oh->lk);
	return oh;

fail:
	woeh_free(oh);
	return NULL;
}

enum WOEH_FLAGS {
	/** Unregister user handle after the first signal (before calling user function) */
	WOEH_F_ONESHOT = 1,
};

/** Associate HANDLE with user-function.
Can be called safely from a user-function.
flags: enum WOEH_FLAGS
Return 0 on success.  On failure, 'errno' may be set to an error from WOH-thread. */
static inline int woeh_add(woeh *oh, HANDLE h, woeh_handler_t handler, void *udata, uint flags)
{
	fflock_lock(&oh->lk);
	if (oh->count == MAXIMUM_WAIT_OBJECTS || oh->count == (uint)-1) {
		if (oh->count == MAXIMUM_WAIT_OBJECTS)
			fferr_set(ERROR_INVALID_PARAMETER);
		else
			fferr_set(oh->thderr);
		fflock_unlock(&oh->lk);
		return 1;
	}
	oh->items[oh->count].handler = handler;

	FF_ASSERT(((size_t)udata & 1) == 0);
	if (flags & WOEH_F_ONESHOT)
		udata = (void*)((size_t)udata | 1);
	oh->items[oh->count].udata = udata;

	oh->hdls[oh->count++] = h;
	fflock_unlock(&oh->lk);

	SetEvent(oh->wake_evt);
	return 0;
}

static int _woeh_find(woeh *oh, HANDLE h)
{
	for (uint i = 1 /*skip wake_evt*/;  i < oh->count;  i++) {
		if (oh->hdls[i] == h)
			return i;
	}
	return -1;
}

/** Unregister HANDLE.
Can be called safely from a user-function. */
static inline int woeh_rm(woeh *oh, HANDLE h)
{
	int rc = -1;
	ffbool is_worker = (ffthread_curid() == oh->tid);

	fflock_lock(&oh->lk);
	int i = _woeh_find(oh, h);
	if (i < 0)
		goto end;

	if (!is_worker) {
		FFINT_WRITEONCE(oh->cmd, WOEH_CMD_WAIT);
		SetEvent(oh->wake_evt);

		//wait until the worker thread returns from the kernel
		WaitForSingleObject(oh->wait_evt, INFINITE);
		//now the worker thread waits until oh->lk is unlocked
	}

	_woeh_rm(oh, i);
	rc = 0;

end:
	fflock_unlock(&oh->lk);
	return rc;
}
