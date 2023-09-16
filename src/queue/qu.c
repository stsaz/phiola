/** phiola: queue
2023, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <ffbase/lock.h>
#include <FFOS/random.h>

extern const phi_core *core;
extern const phi_track_if phi_track_iface;
static const phi_meta_if *phi_metaif;
#define dbglog(...)  phi_dbglog(core, "queue", NULL, __VA_ARGS__)

typedef void (*on_change_t)(phi_queue_id, uint, uint);
struct queue_mgr {
	ffvec lists; // struct phi_queue*[]
	uint selected;
	uint random_ready :1;
	on_change_t on_change;
};
static struct queue_mgr *qm;

struct phi_queue {
	struct phi_queue_conf conf;
	ffvec index; // struct q_entry*[]
	fflock lock;
	struct q_entry *cursor;
	uint cursor_index;
	uint active_n;
	uint random_split :1;
};

enum {
	Q_NONE,
	Q_PLAYING,
};

static int q_play_next(struct phi_queue *q);
static void* q_insert(struct phi_queue *q, uint pos, struct phi_queue_entry *qe);
static int q_remove_at(struct phi_queue *q, uint pos, uint n);
static struct q_entry* q_get(struct phi_queue *q, uint i);
static int q_find(struct phi_queue *q, struct q_entry *e);

#include <queue/ent.h>

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


static void q_free(struct phi_queue *q)
{
	struct q_entry **e;
	FFSLICE_WALK(&q->index, e) {
		qe_unref(*e);
	}
	ffvec_free(&q->index);
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

static void* q_insert(struct phi_queue *q, uint pos, struct phi_queue_entry *qe)
{
	struct q_entry *e = qe_new(qe);
	e->q = q;
	e->index = pos;
	ffvec_pushT(&q->index, void*);
	if (pos+1 == q->index.len)
		*ffslice_lastT(&q->index, void*) = e;
	else
		*ffslice_moveT((ffslice*)&q->index, pos, pos + 1, q->index.len - 1 - pos, void*) = e;
	dbglog("added '%s' [%L]", qe->conf.ifile.name, q->index.len);
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
	if (n == 1)
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
	struct q_entry *e = _e;
	if (!e) {
		if (!q)
			q = qm_default();
		if (!(e = q_get(q, q_get_index(q, q->cursor_index))))
			return -1;
	} else {
		q = e->q;
	}

	if (q->cursor)
		qe_stop(q->cursor);

	q->cursor = e;
	q->cursor_index = qe_index(e);
	if (!!qe_play(e))
		return -1;
	return 0;
}

static int q_play_next(struct phi_queue *q)
{
	dbglog("%s", __func__);
	if (!q) q = qm_default();

	struct q_entry *e = q->cursor;
	if (!!e) {
		uint i = (e->index != ~0U) ? e->index + 1 : q->cursor_index;
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
}

static int q_save_process(void *f, phi_track *t) { return PHI_DONE; }

static const phi_filter phi_qsave_guard = {
	NULL, q_save_close, q_save_process, "qsave-guard"
};

static int q_save(struct phi_queue *q, const char *filename)
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
	phi_track *t = core->track->create(&c);
	t->udata = q;
	if (!core->track->filter(t, &phi_qsave_guard, 0)
		|| !core->track->filter(t, core->mod("format.m3u-write"), 0)
		|| (compress
			&& !core->track->filter(t, core->mod("zstd.compress"), 0))
		|| !core->track->filter(t, core->mod("core.file-write"), 0)) {
		core->track->close(t);
		return -1;
	}
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
	ffslice_rmT((ffslice*)&q->index, pos, 1, void*);
	qe_unref(e);
	fflock_unlock(&q->lock);
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

const phi_queue_if phi_queueif = {
	q_create,
	q_destroy,
	qm_select,
	q_conf,
	qm_qselect,

	q_add,
	q_clear,
	q_count,
	q_filter,

	q_play,
	q_play_next,
	q_play_prev,

	q_save,
	q_status,
	q_at,
	q_remove_at,

	q_ref,
	(void*)qe_unref,

	(void*)qe_insert,
	(void*)qe_index,
	(void*)qe_remove,

	qm_set_on_change,
};
