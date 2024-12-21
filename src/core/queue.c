/** phiola: queue
2023, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <ffbase/lock.h>
#include <ffsys/random.h>

extern const phi_core *core;
extern const phi_track_if phi_track_iface;
extern const phi_meta_if *phi_metaif;
#define dbglog(...)  phi_dbglog(core, "queue", NULL, __VA_ARGS__)
#define errlog(...)  phi_errlog(core, "queue", NULL, __VA_ARGS__)
#define ERR_MAX  20

typedef void (*on_change_t)(phi_queue_id, uint, uint);
struct queue_mgr {
	ffvec lists; // struct phi_queue*[]
	uint selected;
	uint errors;
	int dev_idx;
	uint random_ready :1;
	on_change_t on_change;
	struct q_entry *cursor;
};
static struct queue_mgr *qm;

struct phi_queue {
	struct phi_queue_conf conf;
	ffvec index; // struct q_entry*[]
	fflock lock;
	phi_task task;
	struct q_entry *cursor;
	uint cursor_index;
	uint active_n, finished_n;
	uint track_closed_flags;
	uint closing :1;
	uint random_split :1;
};

enum {
	Q_NONE,
	Q_PLAYING,
};

static void* q_insert(struct phi_queue *q, uint pos, struct phi_queue_entry *qe);
static int q_remove_at(struct phi_queue *q, uint pos, uint n);
static struct q_entry* q_get(struct phi_queue *q, uint i);
static int q_find(struct phi_queue *q, struct q_entry *e);
enum Q_TKCL_F {
	Q_TKCL_ERR = 1,
	Q_TKCL_STOP = 2,
};
static void q_ent_closed(struct phi_queue *q, uint flags);
static void q_modified(struct phi_queue *q);

#include <core/queue-entry.h>

static int q_play_next(struct phi_queue *q);
static void q_on_change(phi_queue_id q, uint flags, uint pos){}
static void q_free(struct phi_queue *q);

void qm_destroy()
{
	struct phi_queue **q;
	FFSLICE_WALK(&qm->lists, q) {
		q_free(*q);
	}
	ffvec_free(&qm->lists);
	ffmem_free(qm);
}

void qm_init()
{
	qm = ffmem_new(struct queue_mgr);
	qm->dev_idx = -1;
	qm->on_change = q_on_change;
}

static void qm_add(struct phi_queue *q)
{
	if (!phi_metaif)
		phi_metaif = core->mod("format.meta");

	*ffvec_pushT(&qm->lists, struct phi_queue*) = q;
	dbglog("added list [%L]", qm->lists.len);
}

static void qm_rm(struct phi_queue *q)
{
	struct phi_queue **it;
	FFSLICE_WALK(&qm->lists, it) {
		if (*it == q) {
			ffslice_rmT((ffslice*)&qm->lists, it - (struct phi_queue**)qm->lists.ptr, 1, void*);
			break;
		}
	}
}

static struct phi_queue* qm_default()
{
	return *ffslice_itemT(&qm->lists, qm->selected, struct phi_queue*);
}

static phi_queue_id qm_select(uint pos)
{
	if (pos >= qm->lists.len) return NULL;
	qm->selected = pos;
	return qm_default();
}

static void qm_qselect(phi_queue_id q)
{
	uint i = 0;
	struct phi_queue **it;
	FFSLICE_WALK(&qm->lists, it) {
		if (*it == q) {
			qm->selected = i;
			return;
		}
		i++;
	}
}

static void qm_set_on_change(on_change_t cb)
{
	qm->on_change = cb;
}

static void qm_move(uint from, uint to)
{
	if (ffmax(from, to) >= qm->lists.len
		|| !(from - 1 == to)) // move left
		return;

	struct phi_queue **l = qm->lists.ptr;
	l[to] = FF_SWAP(&l[from], l[to]);
	if (qm->selected == from)
		qm->selected = to;
	dbglog("move: %u -> %u", from, to);
}


static void q_free(struct phi_queue *q)
{
	struct q_entry **e;
	FFSLICE_WALK(&q->index, e) {
		qe_unref(*e);
	}
	ffvec_free(&q->index);

	if (q->active_n > 0) {
		q->closing = 1;
		return;
	}

	ffmem_free(q->conf.name);
	ffmem_free(q);
}

static struct phi_queue* q_create(struct phi_queue_conf *conf)
{
	struct phi_queue *q = ffmem_new(struct phi_queue);
	q->conf = *conf;
	if (!q->conf.audio_module) q->conf.audio_module = "core.auto-play";
	qm_add(q);
	qm->on_change(q, 'n', 0);
	return q;
}

static void q_destroy(phi_queue_id q)
{
	if (!q) q = qm_default();
	qm_rm(q);
	qm->on_change(q, 'd', 0);
	q_free(q);
}

static struct phi_queue_conf* q_conf(phi_queue_id q)
{
	if (!q) q = qm_default();
	return &q->conf;
}

static void q_modified(struct phi_queue *q)
{
	q->conf.modified = 1;
}

static void* q_insert(struct phi_queue *q, uint pos, struct phi_queue_entry *qe)
{
	struct q_entry *e = qe_new(qe);
	e->q = q;
	e->index = pos;
	fflock_lock(&q->lock);
	ffvec_pushT(&q->index, void*);
	if (pos+1 == q->index.len)
		*ffslice_lastT(&q->index, void*) = e;
	else
		*ffslice_moveT((ffslice*)&q->index, pos, pos + 1, q->index.len - 1 - pos, void*) = e;
	fflock_unlock(&q->lock);
	dbglog("added '%s' [%u/%L]", qe->conf.ifile.name, pos, q->index.len);
	q_modified(q);
	qm->on_change(q, 'a', pos);
	return e;
}

static int q_add(struct phi_queue *q, struct phi_queue_entry *qe)
{
	if (!q) q = qm_default();

	struct q_entry *e = q_insert(q, q->index.len, qe);
	qe_expand(e);
	return e->index;
}

static int q_clear(struct phi_queue *q)
{
	if (!q) q = qm_default();

	fflock_lock(&q->lock);
	ffvec a = q->index;
	ffvec_null(&q->index);
	fflock_unlock(&q->lock);
	q_modified(q);
	qm->on_change(q, 'c', 0);

	struct q_entry **it;
	FFSLICE_WALK(&a, it) {
		struct q_entry *e = *it;
		e->index = ~0;
		qe_unref(e);
	}

	ffvec_free(&a);
	return 0;
}

static int q_count(struct phi_queue *q)
{
	if (!q) q = qm_default();
	return q->index.len;
}

/** Initialize random number generator */
static void qm_rand_init()
{
	if (qm->random_ready) return;

	qm->random_ready = 1;
	fftime t;
	fftime_now(&t);
	ffrand_seed(t.sec);
}

/** Get random index */
static uint q_random(struct phi_queue *q)
{
	ffsize n = q->index.len;
	if (n <= 1)
		return 0;
	qm_rand_init();
	ffsize i = ffrand_get();
	if (!q->random_split)
		i %= n / 2;
	else
		i = n / 2 + (i % (n - (n / 2)));
	q->random_split = !q->random_split;
	return i;
}

static uint q_get_index(struct phi_queue *q, uint i)
{
	if (q->conf.random)
		i = q_random(q);
	else if (i >= q->index.len && q->conf.repeat_all)
		i = 0;
	return i;
}

static int q_find(struct phi_queue *q, struct q_entry *e)
{
	struct q_entry **it;
	FFSLICE_WALK(&q->index, it) {
		if (e == *it)
			return it - (struct q_entry**)q->index.ptr;
	}
	return -1;
}

static struct q_entry* q_get(struct phi_queue *q, uint i)
{
	if (i >= q->index.len) return NULL;
	return *ffslice_itemT(&q->index, i, struct q_entry*);
}

static int q_play(struct phi_queue *q, void *_e)
{
	dbglog("%s", __func__);
	struct q_entry *e = _e;
	if (!e) {
		if (!q)
			q = qm_default();
		if (!(e = q_get(q, q_get_index(q, q->cursor_index))))
			return -1;
	} else {
		q = e->q;
	}

	if (q->conf.conversion) {
		for (;;) {
			q->cursor = e;
			q->cursor_index = qe_index(e);
			if (qe_play(e))
				return -1;

			uint i = e->index + 1;
			if (i >= q->index.len)
				break;

			if (FFINT_READONCE(qm->errors) >= ERR_MAX)
				return -1; // consecutive errors increase while we're starting the tracks here

			if (!core->workers_available())
				break;

			e = q_get(q, i);
		}
		return 0;
	}

	if (qm->cursor)
		qe_stop(qm->cursor);

	qm->cursor = e;
	q->cursor = e;
	q->cursor_index = qe_index(e);
	if (!!qe_play(e))
		return -1;
	return 0;
}

static void q_trk_closed(void *param)
{
	struct phi_queue *q = param;
	fflock_lock(&q->lock);
	uint n = FF_SWAP(&q->finished_n, 0);
	uint flags = FF_SWAP(&q->track_closed_flags, 0);
	fflock_unlock(&q->lock);

	q->active_n -= n;

	if (!(flags & Q_TKCL_STOP)) {
		if (!(q->conf.conversion
			&& core->conf.workers > 1 && !core->workers_available()))
			q_play_next(q);
	}

	if (q->active_n == 0) {
		if (q->closing) {
			q_free(q);
			return;
		}

		qm->errors = 0; // for the user to be able to manually restart the processing
		qm->on_change(q, '.', 0);
	}
}

/**
Thread: worker */
static void q_ent_closed(struct phi_queue *q, uint flags)
{
	dbglog("%s  flags:%u", __func__, flags);

	/* Don't start the next track when there are too many consecutive errors.
	When in Random or Repeat-All mode we may waste CPU resources without making any progress, e.g.:
	* input: storage isn't online, files were moved, etc.
	* filters: format or codec isn't supported, decoding error, etc.
	* output: audio system is failing */
	if (flags & Q_TKCL_ERR) {
		uint e = ffint_fetch_add(&qm->errors, 1) + 1;
		if (e >= ERR_MAX) {
			if (e == ERR_MAX)
				errlog("Stopped after %u consecutive errors", e);
			flags |= Q_TKCL_STOP;
		}
	} else {
		ffint_fetch_and(&qm->errors, 0);
	}

	fflock_lock(&q->lock);
	q->track_closed_flags = flags;
	uint signal = (q->finished_n == 0);
	q->finished_n++;
	FF_ASSERT(q->finished_n <= q->active_n);
	fflock_unlock(&q->lock);

	if (signal) // guarantee that the queue isn't destroyed
		core->task(0, &q->task, q_trk_closed, q);
}

static int q_play_next(struct phi_queue *q)
{
	dbglog("%s", __func__);
	if (!q) q = qm_default();

	struct q_entry *e = q->cursor;
	if (!!e) {
		uint i = (e->index != ~0U) ? (uint)qe_index(e) + 1 : q->cursor_index;
		if (!(e = q_get(q, q_get_index(q, i))))
			return -1;
	}
	return q_play(q, e);
}

static int q_play_prev(struct phi_queue *q)
{
	if (!q) q = qm_default();

	struct q_entry *e = q->cursor;
	if (!!e) {
		uint i = (e->index != ~0U) ? e->index - 1 : q->cursor_index;
		if (!(e = q_get(q, q_get_index(q, i))))
			return -1;
	}
	return q_play(q, e);
}

static int q_status(struct phi_queue *q)
{
	if (!q) q = qm_default();

	uint r = 0;
	if (q->active_n != 0)
		r |= Q_PLAYING;
	return r;
}

static void q_save_close(void *f, phi_track *t)
{
	ffmem_free(t->conf.ofile.name);  t->conf.ofile.name = NULL;
	core->track->stop(t);
	if (t->q_save.on_complete)
		t->q_save.on_complete(t->q_save.param, t);
}

static int q_save_process(void *f, phi_track *t) { return PHI_DONE; }

static const phi_filter phi_qsave_guard = {
	NULL, q_save_close, q_save_process, "qsave-guard"
};

static int q_save(struct phi_queue *q, const char *filename, void (*on_complete)(void*, phi_track*), void *param)
{
	if (!q) q = qm_default();

	ffstr ext = {};
	ffpath_split3_str(FFSTR_Z(filename), NULL, NULL, &ext);
	ffbool compress = ffstr_eqz(&ext, "m3uz");

	struct phi_track_conf c = {
		.ofile = {
			.buf_size = 1*1024*1024,
			.name = ffsz_dup(filename),
			.name_tmp = 1,
			.overwrite = 1,
		},
	};

	int skip = 0;
	fffileinfo fi;
	if (!q->conf.modified && !fffile_info_path(filename, &fi)) {
		fftime mt = fffileinfo_mtime(&fi);
		mt.sec += FFTIME_1970_SECONDS;
		if (!fftime_cmp_val(mt, q->conf.last_mod_time)) {
			skip = 1;
			dbglog("q_save: '%s': skip (mtime)", filename);
		}
	}

	phi_track *t = core->track->create(&c);
	t->udata = q;
	core->track->filter(t, &phi_qsave_guard, 0);
	if (!skip
		&& (!core->track->filter(t, core->mod("format.m3u-write"), 0)
			|| (compress
				&& !core->track->filter(t, core->mod("zstd.compress"), 0))
			|| !core->track->filter(t, core->mod("core.file-write"), 0))) {
		core->track->close(t);
		return -1;
	}

	t->q_save.on_complete = on_complete;
	t->q_save.param = param;
	core->track->start(t);
	return 0;
}

static struct phi_queue_entry* q_at(struct phi_queue *q, uint pos)
{
	if (!q) q = qm_default();

	struct q_entry *qe = q_get(q, pos);
	if (!qe) return NULL;
	return &qe->pub;
}

static struct phi_queue_entry* q_ref(phi_queue_id q, uint pos)
{
	if (!q) q = qm_default();

	fflock_lock(&q->lock);
	struct q_entry *e = q_get(q, pos);
	if (!e) { goto end; }
	e->used++;

end:
	fflock_unlock(&q->lock);
	return (e) ? &e->pub : NULL;
}

static int q_remove_at(struct phi_queue *q, uint pos, uint n)
{
	if (!q) q = qm_default();

	struct q_entry *e = q_get(q, pos);
	if (!e)
		return -1;
	e->index = ~0;
	dbglog("removed '%s' @%u", e->pub.conf.ifile.name, pos);
	fflock_lock(&q->lock); // after q_ref() has read the item @pos, but before 'used++', the item must not be destroyed

	if (q->cursor_index > 0
		&& (pos < q->cursor_index
			|| (pos == q->cursor_index
				&& pos == q->index.len - 1)))
		q->cursor_index--;

	ffslice_rmT((ffslice*)&q->index, pos, 1, void*);
	qe_unref(e);
	fflock_unlock(&q->lock);
	q_modified(q);
	qm->on_change(q, 'r', pos);
	return 0;
}

static phi_queue_id q_filter(phi_queue_id q, ffstr filter, uint flags)
{
	if (!flags) flags = 3;
	if (!q) q = qm_default();

	struct phi_queue *qf = ffmem_new(struct phi_queue);
	qf->conf = q->conf;
	qf->conf.name = ffsz_dup("Filter");
	qm_add(qf);

	struct q_entry **it, *e;
	FFSLICE_WALK(&q->index, it) {
		e = *it;
		if (qe_filter(e, filter, flags))
			*ffvec_pushT(&qf->index, void*) = qe_ref(e);
	}

	return qf;
}

/** Sort the index randomly */
static void sort_random(phi_queue_id q)
{
	qm_rand_init();
	struct q_entry **it, **e = (struct q_entry**)q->index.ptr;
	FFSLICE_WALK(&q->index, it) {
		ffsize n = ffrand_get() % q->index.len;
		void *tmp = *it;
		*it = e[n];
		e[n] = tmp;
	}
}

struct q_sort_params {
	uint flags;
	const struct q_entry **index;
	ffvec file_sizes; // uint64[]
};

static void q_sort_params_destroy(struct q_sort_params *p)
{
	ffvec_free(&p->file_sizes);
}

static int q_sort_cmp(const void *_a, const void *_b, void *udata)
{
	const struct q_entry *a = *(struct q_entry**)_a, *b = *(struct q_entry**)_b;
	const struct q_sort_params *p = udata;

	if (p->flags == PHI_Q_SORT_FILESIZE
		|| p->flags == PHI_Q_SORT_FILEDATE) {
		const uint64 *fs = p->file_sizes.ptr;
		if (fs[a->index] > fs[b->index])
			return -1;
		else if (fs[a->index] < fs[b->index])
			return 1;
	}

	return ffsz_icmp(a->pub.conf.ifile.name, b->pub.conf.ifile.name);
}

static void q_sort(phi_queue_id q, uint flags)
{
	if (!q) q = qm_default();

	if (flags == PHI_Q_SORT_RANDOM) {
		sort_random(q);
	} else {
		struct q_sort_params p = {};
		p.flags = flags;
		p.index = (const struct q_entry**)q->index.ptr;

		if (flags == PHI_Q_SORT_FILESIZE
			|| flags == PHI_Q_SORT_FILEDATE) {
			ffvec_allocT(&p.file_sizes, q->index.len, void*);
			struct q_entry **it;
			uint i = 0;
			FFSLICE_WALK(&q->index, it) {
				fffileinfo fi;
				uint64 fs = 0;
				if (!fffile_info_path((*it)->pub.conf.ifile.name, &fi)) {
					fs = fffileinfo_size(&fi);
					if (flags == PHI_Q_SORT_FILEDATE)
						fs = fffileinfo_mtime(&fi).sec;
				}
				*ffvec_pushT(&p.file_sizes, uint64) = fs;
				(*it)->index = i++;
			}
		}

		ffsort(q->index.ptr, q->index.len, sizeof(void*), q_sort_cmp, &p);

		q_sort_params_destroy(&p);
	}

	dbglog("sorted %L entries", q->index.len);
	q_modified(q);
	qm->on_change(q, 'u', 0);
}

static void qm_device(uint device)
{
	qm->dev_idx = device;
}

static void q_remove_multi(phi_queue_id q, uint flags)
{
	if (!q) q = qm_default();

	ffvec new_index = {};
	ffvec_allocT(&new_index, q->index.len, void*);

	struct q_entry **it;
	FFSLICE_WALK(&q->index, it) {
		struct q_entry *qe = *it;
		if (flags & PHI_Q_RM_NONEXIST) {
			const char *fn = qe->pub.conf.ifile.name;
			if (fffile_exists(fn)) {
				*ffvec_pushT(&new_index, void*) = qe;
				continue;
			} else {
				dbglog("remove: file doesn't exist: '%s'", fn);
			}
		}
		qe_unref(qe);
	}

	if (new_index.len == q->index.len) {
		ffvec_free(&new_index);
		return;
	}

	fflock_lock(&q->lock);
	ffvec old = q->index;
	q->index = new_index;
	fflock_unlock(&q->lock);
	ffvec_free(&old);

	q_modified(q);
	qm->on_change(q, 'u', 0);
}

const phi_queue_if phi_queueif = {
	qm_set_on_change,
	qm_device,

	q_create,
	q_destroy,
	qm_select,
	q_conf,
	qm_qselect,
	qm_move,

	q_add,
	q_count,
	q_filter,

	q_play,
	q_play_next,
	q_play_prev,

	q_save,
	q_status,
	q_sort,

	q_clear,
	q_remove_at,
	q_remove_multi,

	q_at,
	q_ref,
	(void*)qe_unref,

	(void*)qe_queue,
	(void*)qe_insert,
	(void*)qe_index,
	(void*)qe_remove,
};
