/** phiola: manage tracks
2023, Simon Zolin */

#include <track.h>
#include <ffsys/perf.h>
#include <ffbase/list.h>
#include <ffbase/vector.h>

extern struct phi_core *core;
#define syserrlog(t, ...)  phi_syserrlog(core, "track", t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, "track", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "track", t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, "track", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "track", t, __VA_ARGS__)
#define extralog(t, ...)  phi_extralog(core, "track", t, __VA_ARGS__)

static void conveyor_close(struct phi_conveyor *v, phi_track *t);
static void track_wake(phi_track *t);

enum STATE {
	/** Track is running normally */
	ST_RUNNING, // ->(ST_FINISHED || ST_STOP)

	/** Track has finished the work before user calls stop() */
	ST_FINISHED,

	/** User calls stop() on an active track */
	ST_STOP, // ->ST_STOPPING

	/** User's stop-signal is being processed by the track filters */
	ST_STOPPING,
};

struct track_ctx {
	fflist tracks;
	fflock tracks_lock;
	uint cur_id;
};
static struct track_ctx *tx;

void tracks_init()
{
	tx = ffmem_new(struct track_ctx);
	fflist_init(&tx->tracks);
	tx->cur_id = 1;
}

void tracks_destroy()
{
	ffmem_free(tx);  tx = NULL;
}

static int track_conf(struct phi_track_conf *conf)
{
	ffmem_zero_obj(conf);
	return 0;
}

/** Print the time we spent inside each filter */
static void track_busytime_print(phi_track *t)
{
	fftime total = core->time(NULL, PHI_CORE_TIME_MONOTONIC);
	fftime_sub(&total, &t->t_start);

	ffvec buf = {};
	ffvec_addfmt(&buf, "total time: %u.%06u;  busy time: "
		, (int)fftime_sec(&total), (int)fftime_usec(&total));

	struct filter *f;
	FF_FOREACH(t->conveyor.filters_pool, f) {
		if (fftime_empty(&f->busytime))
			continue;
		uint percent = fftime_to_usec(&f->busytime) * 100 / fftime_to_usec(&total);
		ffvec_addfmt(&buf, "%s: %u.%06u (%u%%), "
			, f->iface.name, (int)fftime_sec(&f->busytime), (int)fftime_usec(&f->busytime)
			, percent);
	}
	buf.len -= FFS_LEN(", ");

	infolog(t, "%S", &buf);
	ffvec_free(&buf);
}

static void track_close(phi_track *t)
{
	if (t == NULL) return;

	core->task(t->worker, &t->task_wake, NULL, NULL);
	core->worker_release(t->worker);

	if (t->sib.next) {
		fflock_lock(&tx->tracks_lock);
		fflist_rm(&tx->tracks, &t->sib);
		fflock_unlock(&tx->tracks_lock);
	}

	conveyor_close(&t->conveyor, t);

	if (t->conf.print_time)
		track_busytime_print(t);

	dbglog(t, "closed.  area:%u/%u", t->area_size, t->area_cap);
	ffmem_alignfree(t);
}

static void conveyor_init(struct phi_conveyor *v)
{
	v->cur = ~0;
	v->filters.ptr = v->filters_active;
}

static void conveyor_close(struct phi_conveyor *v, phi_track *t)
{
	for (int i = MAX_FILTERS - 1;  i >= 0;  i--) {
		struct filter *f = &v->filters_pool[i];
		if (f->obj != NULL) {
			extralog(t, "%s: closing", f->iface.name);
			if (f->iface.close) {

				((void**)v->filters.ptr)[0] = f;
				v->cur = 0; // logger needs to know the current filter name

				f->iface.close(f->obj, t);
			}
			f->obj = NULL;
		}
	}
}

static char* conveyor_print(struct phi_conveyor *v, const struct filter *mark, char *buf, ffsize cap)
{
	cap--;
	ffstr s = FFSTR_INITN(buf, 0);
	const struct filter **pf, *f;
	FFSLICE_WALK(&v->filters, pf) {
		f = *pf;
		if (f == mark)
			ffstr_addchar(&s, cap, '*');
		ffstr_addfmt(&s, cap, "%s -> ", f->iface.name);
	}
	if (s.len > FFS_LEN(" -> "))
		s.len -= FFS_LEN(" -> ");
	s.ptr[s.len] = '\0';
	return buf;
}

static phi_track* track_create(struct phi_track_conf *conf)
{
	FF_ASSERT(sizeof(phi_track) < 4000);
	phi_track *t = ffmem_align(4000, 4096);
	ffmem_zero(t, 4000);
	t->area_cap = 4000 - sizeof(phi_track);
	FF_ASSERT(!((size_t)t->area & 7));
	FF_ASSERT(!(t->area_cap & 7));
	t->conf = *conf;
	conveyor_init(&t->conveyor);
	t->worker = core->worker_assign(conf->cross_worker_assign);

	uint id = ffint_fetch_add(&tx->cur_id, 1);
	t->id[0] = '*';
	ffs_fromint(id, t->id+1, sizeof(t->id)-1, 0);

	t->t_start = core->time(NULL, PHI_CORE_TIME_MONOTONIC);

	t->audio.seek = ~0ULL;
	if (t->conf.seek_msec) {
		t->audio.seek = t->conf.seek_msec;
		t->audio.seek_req = 1;
	}
	return t;
}

/**
Return filter index within chain */
static int trk_filter_add(phi_track *t, const phi_filter *iface, uint pos)
{
	struct phi_conveyor *v = &t->conveyor;
	if (v->i_fpool == MAX_FILTERS) {
		errlog(t, "max filters limit reached while trying to add '%s'", iface->name);
		return -1;
	}

	struct filter *f = v->filters_pool + v->i_fpool;
	v->i_fpool++;
	f->iface = *iface;

	v->filters.len++;
	struct filter **pf;
	if ((int)pos < 0 || pos+1 == v->filters.len)
		pf = ffslice_lastT(&v->filters, struct filter*);
	else
		pf = ffslice_moveT(&v->filters, pos, pos+1, v->filters.len - 1 - pos, struct filter*);
	*pf = f;

	char buf[200];
	dbglog(t, "%s: added to chain [%s]"
		, f->iface.name, conveyor_print(v, f, buf, sizeof(buf)));
	return pf - (struct filter**)v->filters.ptr;
}

static int track_filter(phi_track *t, const phi_filter *f, uint flags)
{
	if (f == NULL) return 0;

	struct phi_conveyor *v = &t->conveyor;

	uint pos = v->cur + 1;
	if (flags == PHI_TF_PREV)
		pos = v->cur;
	if (v->cur == ~0U)
		pos = -1;

	int r = trk_filter_add(t, f, pos);
	if (flags == PHI_TF_PREV && (uint)r == v->cur && r >= 0)
		v->cur++; // the current filter added a new filter before it
	return r + 1;
}

/** Call filter */
static int trk_filter_run(phi_track *t, struct filter *f)
{
	struct phi_conveyor *v = &t->conveyor;

	if (f->backward_skip) {
		// last time the filter returned PHI_OK
		f->backward_skip = 0;
		if (v->cur != 0)
			return PHI_MORE; // go to previous filter
		// calling first-in-chain filter
	}

	t->chain_flags &= ~PHI_FFIRST;
	if (v->cur == 0)
		t->chain_flags |= PHI_FFIRST;

	if (f->obj == NULL) {
		extralog(t, "%s: opening", f->iface.name);
		void *obj = (void*)-1;
		if (f->iface.open != NULL) {
			if (PHI_OPEN_ERR == (obj = f->iface.open(t))) {
				extralog(t, "%s: open failed", f->iface.name);
				return PHI_ERR;
			}
			if (obj == PHI_OPEN_SKIP) {
				extralog(t, "%s: skipping", f->iface.name);
				t->data_out = t->data_in;
				return PHI_DONE;
			}
		}
		f->obj = obj;
	}

	extralog(t, "%s: in:%L"
		, f->iface.name, t->data_in.len, t->chain_flags);

	int r = f->iface.process(f->obj, t);

	static const char filter_result_str[][16] = {
		"PHI_DATA",
		"PHI_OK",
		"PHI_DONE",
		"PHI_LASTOUT",
		"PHI_MORE",
		"PHI_BACK",
		"PHI_ASYNC",
		"PHI_FIN",
		"PHI_ERR",
	};
	extralog(t, "%s: %s, out:%L"
		, f->iface.name, filter_result_str[r], t->data_out.len);

	return r;
}

/** Handle result code and output data from filter */
static int trk_filter_handle_result(phi_track *t, struct filter *f, int r)
{
	struct phi_conveyor *v = &t->conveyor;
	uint chain_modified = 0;

	switch (r) {
	case PHI_DONE:
		// deactivate filter
		ffslice_rmT(&v->filters, v->cur, 1, void*);
		v->cur--;
		chain_modified = 1;
		if (v->filters.len == 0)
			return PHI_FIN;

		r = PHI_DATA;
		goto go_fwd;

	case PHI_LASTOUT:
		// deactivate this and all previous filters
		ffslice_rmT(&v->filters, 0, v->cur + 1, void*);
		v->cur = -1;
		chain_modified = 1;
		if (v->filters.len == 0)
			return PHI_FIN;

		r = PHI_DATA;
		goto go_fwd;

	case PHI_OK:
		f->backward_skip = 1;
		r = PHI_DATA;
		// fallthrough

	case PHI_DATA:
go_fwd:
		if (v->cur + 1 == v->filters.len) {
			// last-in-chain filter returned data
			r = PHI_MORE;
			break;
		}
		v->cur++;
		break;

	case PHI_MORE:
	case PHI_BACK:
		if (v->cur == 0) {
			errlog(t, "%s: first-in-chain filter wants more data", f->iface.name);
			return PHI_ERR;
		}
		v->cur--;
		break;

	case PHI_ASYNC:
	case PHI_FIN:
	case PHI_ERR:
		break;

	default:
		errlog(t, "%s: bad return code %u", f->iface.name, r);
		r = PHI_ERR;
	}

	if (chain_modified) {
		char buf[200];
		dbglog(t, "chain (%L): [%s]"
			, t->conveyor.filters.len, conveyor_print(&t->conveyor, NULL, buf, sizeof(buf)));
	}

	return r;
}

static void track_run(phi_track *t)
{
	if (t->state == ST_FINISHED) {
		track_close(t);
		return;
	}

	int r;
	for (;;) {

		if (FFINT_READONCE(t->state) == ST_STOP) {
			// notify active filters so they can stop the chain
			dbglog(t, "finalizing");
			t->state = ST_STOPPING;
			t->chain_flags |= PHI_FSTOP;
		}

		fftime t1, t2;
		if (t->conf.print_time)
			t1 = core->time(NULL, PHI_CORE_TIME_MONOTONIC);

		struct filter *f = *ffslice_itemT(&t->conveyor.filters, t->conveyor.cur, struct filter*);
		r = trk_filter_run(t, f);

		if (t->conf.print_time) {
			t2 = core->time(NULL, PHI_CORE_TIME_MONOTONIC);
			fftime_sub(&t2, &t1);
			fftime_add(&f->busytime, &t2);
		}

		r = trk_filter_handle_result(t, f, r);
		switch (r) {
		case PHI_MORE:
			t->chain_flags &= ~PHI_FFWD;
			ffstr_null(&t->data_in);
			ffstr_null(&t->data_out);
			break;

		case PHI_BACK:
			t->chain_flags &= ~PHI_FFWD;
			t->data_in = t->data_out;
			ffstr_null(&t->data_out);
			break;

		case PHI_DATA:
			t->chain_flags |= PHI_FFWD;
			t->data_in = t->data_out;
			ffstr_null(&t->data_out);
			break;

		case PHI_ASYNC:
			return;

		case PHI_FIN:
			goto fin;

		case PHI_ERR:
			goto err;
		}
	}

err:
	if (t->error == 0)
		t->error = PHI_E_OTHER;

fin:
	dbglog(t, "finished: %u", t->error);
	if (ST_RUNNING == ffint_cmpxchg(&t->state, ST_RUNNING, ST_FINISHED)) {
		t->chain_flags |= PHI_FFINISHED;
		conveyor_close(&t->conveyor, t);
		return; // waiting for stop()
	}
	track_close(t);
}

static void track_start(phi_track *t)
{
	if (!t->conveyor.filters.len)
		return;
	t->conveyor.cur = 0;

	fflock_lock(&tx->tracks_lock);
	fflist_add(&tx->tracks, &t->sib);
	fflock_unlock(&tx->tracks_lock);

	dbglog(t, "starting");
	core->task(t->worker, &t->task_wake, (phi_task_func)track_run, t);
}

static void track_xstop(phi_track *t)
{
	dbglog(t, "stop");
	int st = ffint_cmpxchg(&t->state, ST_RUNNING, ST_STOP);
	// ST_RUNNING: track filters will stop their work, then the track will be closed
	// ST_FINISHED: just close the track
	if (st == ST_RUNNING || st == ST_FINISHED)
		core->task(t->worker, &t->task_wake, (phi_task_func)track_run, t);
}

static void track_wake(phi_track *t)
{
	dbglog(t, "wake up");
	core->task(t->worker, &t->task_wake, (phi_task_func)track_run, t);
}

/**
Return N of tracks that were issued the stop command */
static uint track_xstop_all()
{
	uint n = tx->tracks.len;
	fflock_lock(&tx->tracks_lock);
	ffchain_item *it;
	FFLIST_WALK(&tx->tracks, it) {
		phi_track *t = FF_STRUCTPTR(phi_track, sib, it);
		track_xstop(t);
	}
	fflock_unlock(&tx->tracks_lock);
	return n;
}

static ffssize track_cmd(phi_track *t, uint cmd, ...)
{
	switch (cmd) {
	case PHI_TRACK_STOP_ALL:
		return track_xstop_all();

	case PHI_TRACK_CUR_FILTER_NAME: {
		if (t->conveyor.cur == ~0U) return (ffssize)"";

		struct filter *f = *ffslice_itemT(&t->conveyor.filters, t->conveyor.cur, struct filter*);
		return (ffssize)f->iface.name;
	}
	}
	return 0;
}

phi_track_if phi_track_iface = {
	track_conf,
	track_create,
	track_close,
	track_filter,
	track_start,
	track_xstop,
	track_wake,
	track_cmd,
};
