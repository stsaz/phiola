/** phiola: queue entry
2023, Simon Zolin */

#include <ffsys/path.h>

struct q_entry {
	struct phi_queue_entry pub;
	struct phi_queue *q;
	phi_track *trk;
	uint index;
	uint used;
	uint have_user_meta :1;
	uint expand :1;
	uint play_next_on_close :1;
};

static void meta_destroy(ffvec *meta)
{
	char **it;
	FFSLICE_FOR(meta, it) {
		ffmem_free(*it);
		it += 2;
	}
	ffvec_free(meta);
}

static inline void track_conf_destroy(struct phi_track_conf *c)
{
	ffmem_free(c->ifile.name);  c->ifile.name = NULL;
	ffmem_free(c->ofile.name);  c->ofile.name = NULL;
	ffslice_free(&c->tracks);
	meta_destroy(&c->meta);
}

static void qe_free(struct q_entry *e)
{
	if (e->q && e == e->q->cursor)
		e->q->cursor = NULL;
	if (e == qm->cursor)
		qm->cursor = NULL;

	track_conf_destroy(&e->pub.conf);
	ffmem_free(e);
}

static struct q_entry* qe_new(struct phi_queue_entry *qe)
{
	struct q_entry *e = ffmem_new(struct q_entry);
	e->pub = *qe;
	e->have_user_meta = (e->pub.conf.meta.len && !e->pub.conf.meta_transient);
	e->used = 1;
	return e;
}

struct q_entry* qe_ref(struct q_entry *e)
{
	e->used++;
	return e;
}

int qe_unref(struct q_entry *e)
{
	FF_ASSERT(e->used);
	if (1 != ffint_fetch_add(&e->used, -1))
		return 1;

	qe_free(e);
	return 0;
}

static void* qe_open(phi_track *t) { return t->qent; }

static void qe_close(void *f, phi_track *t)
{
	struct q_entry *e = f;
	if (e->trk == t) {
		e->trk = NULL;

		if (e->q && !e->q->conf.conversion && !e->have_user_meta) {
			int mod = (e->pub.conf.meta.len || t->meta.len); // empty meta == not modified
			if (t->meta.len) {
				fflock_lock((fflock*)&e->pub.lock); // UI thread may read or write `conf.meta` at this moment
				ffvec meta_old = e->pub.conf.meta;
				e->pub.conf.meta = t->meta; // Remember the tags we read from file in this track
				fflock_unlock((fflock*)&e->pub.lock);

				meta_destroy(&meta_old);
				ffvec_null(&t->meta);
			}
			if (mod)
				q_modified(e->q);
		}

		if (e->expand)
			core->track->stop(t);
	}

	meta_destroy(&t->meta);
	if (e->q) {
		uint flags = (t->error) ? Q_TKCL_ERR : 0;
		if ((t->chain_flags & PHI_FSTOP) // track stopped by user
			|| (e->expand && !e->play_next_on_close))
			flags |= Q_TKCL_STOP;
		q_ent_closed(e->q, flags);
	}
	qe_unref(e);
}

static int qe_process(void *f, phi_track *t)
{
	t->data_out = t->data_in;
	return PHI_DONE;
}

static const phi_filter phi_queue_guard = {
	qe_open, qe_close, qe_process,
	"queue-guard"
};

static int qe_play(struct q_entry *e)
{
	if (e->expand) {
		e->play_next_on_close = 1; // wait until expanding is finished and then play next track
		dbglog("play_next_on_close = 1");
		return 1;
	}

	struct phi_track_conf *c = &e->pub.conf;
	c->cross_worker_assign = e->q->conf.conversion;
	const phi_filter *ui_if = (e->q->conf.ui_module_if_set) ? e->q->conf.ui_module_if : core->mod(e->q->conf.ui_module);

	const phi_track_if *track = core->track;
	phi_track *t = track->create(c);

	if (e->q->conf.first_filter
		&& !track->filter(t, e->q->conf.first_filter, 0))
		goto err;

	if (e->q->conf.conversion) {
		if (!track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| (c->afilter.danorm
				&& !track->filter(t, core->mod("danorm.f"), 0))
			|| !track->filter(t, ui_if, 0)
			|| !track->filter(t, core->mod("afilter.gain"), 0)
			|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
			|| !track->filter(t, core->mod("format.auto-write"), 0)
			|| !track->filter(t, core->mod("core.auto-output"), 0))
			goto err;
		t->output.allow_async = 1;

	} else if (c->info_only || c->afilter.peaks_info) {
		if (!track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| !track->filter(t, ui_if, 0)
			|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
			|| (c->afilter.peaks_info
				&& !track->filter(t, core->mod("afilter.peaks"), 0)))
			goto err;

	} else {
		if (!track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| (c->tee
				&& !track->filter(t, core->mod("core.tee"), 0))
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| (c->afilter.danorm
				&& !track->filter(t, core->mod("danorm.f"), 0))
			|| !track->filter(t, ui_if, 0)
			|| !track->filter(t, core->mod("afilter.gain"), 0)
			|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
			|| (c->tee_output
				&& !track->filter(t, core->mod("core.tee"), 0))
			|| !track->filter(t, core->mod(e->q->conf.audio_module), 0))
			goto err;
	}

	if (e->have_user_meta) {
		phi_metaif->copy(&t->meta, &t->conf.meta);
	}

	if (qm->dev_idx >= 0)
		t->conf.oaudio.device_index = qm->dev_idx;

	e->trk = t;
	e->used++;
	e->q->active_n++;

	t->qent = &e->pub;
	track->start(t);
	return 0;

err:
	track->close(t);
	return -1;
}

static int qe_expand(struct q_entry *e)
{
	struct phi_track_conf *c = &e->pub.conf;
	const phi_track_if *track = core->track;

	ffbool dir = 0, decompress = 0;
	fffileinfo fi;
	if (!fffile_info_path(c->ifile.name, &fi)
		&& fffile_isdir(fffileinfo_attr(&fi))) {
		dir = 1;
	} else {
		ffstr ext;
		ffpath_splitname_str(FFSTR_Z(c->ifile.name), NULL, &ext);
		if (!(ffstr_eqz(&ext, "m3u8")
			|| ffstr_eqz(&ext, "m3u")
			|| (decompress = ffstr_eqz(&ext, "m3uz"))))
			return -1;
	}

	phi_track *t = track->create(c);
	if (dir) {
		if (!core->track->filter(t, &phi_queue_guard, 0)
			|| !core->track->filter(t, core->mod("core.dir-read"), 0))
			goto err;
	} else {
		if (!core->track->filter(t, &phi_queue_guard, 0)
			|| !core->track->filter(t, core->mod("core.auto-input"), 0)
			|| (decompress
				&& !core->track->filter(t, core->mod("zstd.decompress"), 0))
			|| !core->track->filter(t, core->mod("format.m3u"), 0))
			goto err;
	}

	e->expand = 1;
	e->trk = t;
	e->used++;
	e->q->active_n++;

	t->qent = &e->pub;
	track->start(t);
	return 0;

err:
	track->close(t);
	return -1;
}

static void qe_stop(struct q_entry *e)
{
	if (e->trk)
		core->track->stop(e->trk);
}

static int qe_index(struct q_entry *e)
{
	if (!e->q) return -1;

	if (q_get(e->q, e->index) != e) {
		e->index = q_find(e->q, e);
	}
	return e->index;
}

static void* qe_insert(struct q_entry *e, struct phi_queue_entry *qe)
{
	return q_insert(e->q, qe_index(e) + 1, qe);
}

static int qe_remove(struct q_entry *e)
{
	return q_remove_at(e->q, qe_index(e), 1);
}

/** Check if the item matches the filter */
int qe_filter(struct q_entry *e, ffstr filter, uint flags)
{
	ffstr name, val;

	if (flags & 1) {
		name = FFSTR_Z(e->pub.conf.ifile.name);
		if (ffstr_ifindstr(&name, &filter) >= 0)
			return 1;
	}

	if (flags & 2) {
		uint i = 0;
		while (phi_metaif->list(&e->pub.conf.meta, &i, &name, &val, 0)) {
			if (ffstr_ifindstr(&val, &filter) >= 0)
				return 1;
		}
	}

	return 0;
}

static phi_queue_id qe_queue(struct q_entry *e)
{
	return e->q;
}
