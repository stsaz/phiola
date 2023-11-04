/** Process events from kernel queue
2022, Simon Zolin */

/*
zzkq_create zzkq_destroy
zzkq_kev_alloc
zzkq_kev_free
zzkq_stop
zzkq_run
zzkq_attach
*/

#pragma once
#include <ffsys/queue.h>
#include <ffsys/kcall.h>

typedef void (*zzkevent_func)(void *obj);
struct zzkevent {
	zzkevent_func rhandler, whandler;
	union {
		ffkq_task rtask;
		ffkq_task_accept rtask_accept;
	};
	ffkq_task wtask;
	uint side;
	void *obj;
	struct zzkevent *prev_kev;
	struct ffkcall kcall;
};


// User code must define these values:
// #define ZZKQ_LOG_SYSERR  ?
// #define ZZKQ_LOG_ERR  ?
// #define ZZKQ_LOG_DEBUG  ?
// #define ZZKQ_LOG_EXTRA  ?

#define zzkq_syserrlog(k, ...) \
	k->conf.log(k->conf.log_obj, ZZKQ_LOG_SYSERR, k->conf.log_ctx, NULL, __VA_ARGS__)

#define zzkq_errlog(k, ...) \
	k->conf.log(k->conf.log_obj, ZZKQ_LOG_ERR, k->conf.log_ctx, NULL, __VA_ARGS__)

#define zzkq_dbglog(k, ...) \
do { \
	if (k->conf.log_level >= ZZKQ_LOG_DEBUG) \
		k->conf.log(k->conf.log_obj, ZZKQ_LOG_DEBUG, k->conf.log_ctx, NULL, __VA_ARGS__); \
} while (0)

#define zzkq_extralog(k, ...)
#ifdef ZZKQ_LOG_EXTRA
	#undef zzkq_extralog
	#define zzkq_extralog(k, ...) \
	do { \
		if (k->conf.log_level >= ZZKQ_LOG_DEBUG) \
			k->conf.log(k->conf.log_obj, ZZKQ_LOG_EXTRA, k->conf.log_ctx, NULL, __VA_ARGS__); \
	} while (0)
#endif


typedef void (*log_func)(void *obj, uint flags, const char *ctx, phi_track *trk, const char *fmt, ...);

struct zzkq_conf {
	uint log_level;
	log_func log;
	void *log_obj;
	const char *log_ctx;

	uint max_objects;
	uint events_wait;
	uint polling_mode :1;
};

struct zzkq {
	struct zzkq_conf conf;

	fffd kq;
	ffkq_event *events;
	uint stop;

	struct zzkevent *kevs;
	struct zzkevent *kevs_unused_lifo;
	uint kevs_allocated, kevs_locked;

	struct zzkevent post_kev;
	ffkq_postevent kqpost;
};

static inline void zzkq_init(struct zzkq *k)
{
	k->kq = FFKQ_NULL;
	k->kqpost = FFKQ_NULL;
}

static inline int zzkq_create(struct zzkq *k, struct zzkq_conf *conf)
{
	if (FFKQ_NULL == (k->kq = ffkq_create())) {
		zzkq_syserrlog(k, "ffkq_create");
		return -1;
	}

	if (FFKQ_NULL == (k->kqpost = ffkq_post_attach(k->kq, &k->post_kev))) {
		zzkq_syserrlog(k, "ffkq_post_attach");
		goto err;
	}

	if (NULL == (k->kevs = ffmem_zalloc(conf->max_objects * sizeof(struct zzkevent)))
		|| NULL == (k->events = ffmem_alloc(conf->events_wait * sizeof(ffkq_event))))
		goto err;

	k->conf = *conf;
	return 0;

err:
	ffmem_free(k->kevs);
	ffmem_free(k->events);
	ffkq_close(k->kq);
	return -1;
}

static inline void zzkq_destroy(struct zzkq *k)
{
	ffkq_post_detach(k->kqpost, k->kq);  k->kqpost = FFKQ_NULL;
	ffkq_close(k->kq);  k->kq = FFKQ_NULL;
	ffmem_free(k->events);  k->events = NULL;
	ffmem_free(k->kevs);  k->kevs = NULL;
}

/** Get next zzkevent object
Return NULL if there's no free slot */
static inline struct zzkevent* zzkq_kev_alloc(struct zzkq *k)
{
	struct zzkevent *kev = k->kevs_unused_lifo;
	if (kev != NULL) {
		k->kevs_unused_lifo = kev->prev_kev;
		kev->prev_kev = NULL;
	} else {
		if (k->kevs_allocated == k->conf.max_objects) {
			zzkq_errlog(k, "reached max objects limit");
			return NULL;
		}
		kev = &k->kevs[k->kevs_allocated++];
	}
	k->kevs_locked++;
	zzkq_dbglog(k, "using kevent slot #%u [%u]"
		, (uint)(kev - k->kevs), k->kevs_locked);
	return kev;
}

static inline void zzkq_kev_free(struct zzkq *k, struct zzkevent *kev)
{
	kev->rhandler = NULL;
	kev->whandler = NULL;
	kev->rtask.active = 0;
	kev->wtask.active = 0;
	kev->obj = NULL;
	kev->side = !kev->side;

	kev->prev_kev = k->kevs_unused_lifo;
	k->kevs_unused_lifo = kev;

	FF_ASSERT(k->kevs_locked != 0);
	k->kevs_locked--;
	zzkq_dbglog(k, "free kevent slot #%u [%u]"
		, (uint)(kev - k->kevs), k->kevs_locked);
}

static inline void zzkq_stop(struct zzkq *k)
{
	if (k->stop) return;

	zzkq_dbglog(k, "stopping kq worker");
	FFINT_WRITEONCE(k->stop, 1);
	ffkq_post(k->kqpost, &k->post_kev);
}

#define _zzkq_kev_data_attach(kev)  (void*)((ffsize)kev | kev->side)

#define _zzkq_kev_data_retrieve(d) \
({ \
	struct zzkevent *kev = (void*)((ffsize)(d) & ~1); \
	if (((ffsize)(d) & 1) != kev->side) \
		kev = NULL; \
	kev; \
})

static inline int zzkq_attach(struct zzkq *k, fffd fd, struct zzkevent *kev, uint flags)
{
	return ffkq_attach(k->kq, fd, _zzkq_kev_data_attach(kev), flags);
}

static void _zzkq_kev_call(struct zzkq *k, struct zzkevent *kev, ffkq_event *ev)
{
	uint flags = ffkq_event_flags(ev);

#ifdef FF_WIN
	flags = FFKQ_READ;
	if (ev->lpOverlapped == &kev->wtask.overlapped)
		flags = FFKQ_WRITE;
#endif

	zzkq_extralog(k, "%p #%D f:%xu r:%d w:%d"
		, kev, (kev > k->kevs) ? (ffint64)(kev - k->kevs) : -1LL, flags, kev->rtask.active, kev->wtask.active);

	if ((flags & FFKQ_READ) && kev->rtask.active) {
		ffkq_task_event_assign(&kev->rtask, ev);
		kev->rhandler(kev->obj);
	}

	if ((flags & FFKQ_WRITE) && kev->wtask.active) {
		ffkq_task_event_assign(&kev->wtask, ev);
		kev->whandler(kev->obj);
	}
}

static inline int zzkq_run(struct zzkq *k)
{
	zzkq_dbglog(k, "entering kq loop");

	ffkq_time t;
	ffkq_time_set(&t, -1);
	if (k->conf.polling_mode)
		ffkq_time_set(&t, 0);

	while (!FFINT_READONCE(k->stop)) {

		int r = ffkq_wait(k->kq, k->events, k->conf.events_wait, t);

		for (int i = 0;  i < r;  i++) {

			ffkq_event *ev = &k->events[i];
			void *d = ffkq_event_data(ev);
			struct zzkevent *kev = _zzkq_kev_data_retrieve(d);
			if (kev != NULL)
				_zzkq_kev_call(k, kev, ev);
		}

#ifdef FF_WIN
		if (r < 0)
#else
		if (r < 0 && fferr_last() != EINTR)
#endif
		{
			zzkq_syserrlog(k, "ffkq_wait");
			return -1;
		}

		if (r > 1) {
			zzkq_extralog(k, "processed %u events", r);
		}
	}

	zzkq_dbglog(k, "leaving kq loop");
	return 0;
}
