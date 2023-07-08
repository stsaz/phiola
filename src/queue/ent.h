/** phiola: queue entry
2023, Simon Zolin */

#include <FFOS/path.h>

struct q_entry {
	struct phi_queue_entry pub;
	struct phi_queue *q;
	phi_track *trk;
	uint index;
	uint used;
	uint expand :1;
	uint play_next_on_close :1;
};

static void meta_destroy(ffvec *meta)
{
	char **it;
	FFSLICE_WALK(meta, it) {
		ffmem_free(*it);
	}
	ffvec_free(meta);
}

static inline void track_conf_destroy(struct phi_track_conf *c)
{
	ffmem_free(c->ifile.name);
	ffmem_free(c->ofile.name);
	meta_destroy(&c->meta);
}

static void qe_free(struct q_entry *e)
{
	track_conf_destroy(&e->pub.conf);
	ffmem_free(e);
}

static struct q_entry* qe_new(struct phi_queue_entry *qe)
{
	struct q_entry *e = ffmem_new(struct q_entry);
	e->pub = *qe;
	e->used = 1;
	return e;
}

static void qe_unref(struct q_entry *e)
{
	if (--e->used != 0) return;

	qe_free(e);
}

static void* qe_open(phi_track *t) { return t->qent; }

static void qe_close(void *f, phi_track *t)
{
	struct q_entry *e = f;
	if (e->trk == t) {
		e->trk = NULL;

		if (!e->q->conf.conversion) {
			meta_destroy(&e->pub.conf.meta);
			e->pub.conf.meta = t->meta; // Remember the tags we read from file in this track
			ffvec_null(&t->meta);
		}
	}
	e->q->active_n--;
	if (!(t->chain_flags & PHI_FSTOP)
		&& (!e->expand || e->play_next_on_close))
		q_play_next(e->q);
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
	const phi_track_if *track = core->track;
	phi_track *t = track->create(c);

	if (e->q->conf.conversion) {
		if (!track->filter(t, e->q->conf.first_filter, 0)
			|| !track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| !track->filter(t, core->mod("tui.play"), 0)
			|| !track->filter(t, core->mod("afilter.gain"), 0)
			|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
			|| !track->filter(t, core->mod("format.auto-write"), 0)
			|| !track->filter(t, core->mod("core.auto-output"), 0))
			goto err;

		char **it;
		FFSLICE_WALK(&t->conf.meta, it) {
			phi_metaif->set(&t->meta, FFSTR_Z(*it), FFSTR_Z(*(it + 1)));
			it++;
		}

	} else if (c->info_only || c->afilter.peaks_info) {
		if (!track->filter(t, e->q->conf.first_filter, 0)
			|| !track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| !track->filter(t, core->mod("tui.play"), 0)
			|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
			|| (c->afilter.peaks_info
				&& !track->filter(t, core->mod("afilter.peaks"), 0)))
			goto err;

	} else {
		if (!track->filter(t, e->q->conf.first_filter, 0)
			|| !track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| !track->filter(t, core->mod("tui.play"), 0)
			|| !track->filter(t, core->mod("afilter.gain"), 0)
			|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
			|| !track->filter(t, core->mod(e->q->conf.audio_module), 0))
			goto err;
	}

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

	int dir = 0;
	fffileinfo fi;
	if (!fffile_info_path(c->ifile.name, &fi)
		&& fffile_isdir(fffileinfo_attr(&fi))) {
		dir = 1;
	} else {
		ffstr ext;
		ffpath_splitname_str(FFSTR_Z(c->ifile.name), NULL, &ext);
		if (!(ffstr_eqz(&ext, "m3u8")
			|| ffstr_eqz(&ext, "m3u")
			|| ffstr_eqz(&ext, "m3uz")))
			return -1;
	}

	phi_track *t = track->create(c);
	if (dir) {
		if (!core->track->filter(t, core->mod("core.dir-read"), 0))
			goto err;
	} else {
		if (!core->track->filter(t, &phi_queue_guard, 0)
			|| !core->track->filter(t, core->mod("core.file-read"), 0)
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
