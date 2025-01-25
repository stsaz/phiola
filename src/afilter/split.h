/** phiola: '-split' filter
2024, Simon Zolin */

/*
iaudio -> split (#1)
               \
                -> split-brg -> autoconv -> encoder -> oformat -> file.write (#2)
                ...
*/

#include <track.h>
#include <afilter/pcm.h>

struct split_brg {
	uint	users;
	uint	state;
	ffstr	data;
	phi_track*	parent;
};

enum {
	S_NONE,
	S_PENDING,
	S_LOCKED,
};

static void split_brg_write(struct split_brg *g, ffstr d)
{
	FF_ASSERT(g->state == S_NONE);
	FF_ASSERT(g->data.ptr == NULL);
	g->data = d;
	g->state = S_PENDING;
}

static void split_brg_consume(struct split_brg *g)
{
	ffstr_null(&g->data);
	g->state = S_NONE;
	if (g->parent)
		core->track->wake(g->parent);
}

static int split_brg_busy(struct split_brg *g)
{
	return (g->state != S_NONE);
}

static void split_brg_unref(struct split_brg *g)
{
	if (--g->users == 0)
		ffmem_free(g);
}

static void* split_brg_open(phi_track *t)
{
	struct split_brg *g = t->udata;
	return g;
}

static void split_brg_close(void *f, phi_track *t)
{
	struct split_brg *g = f;
	g->state = S_NONE;
	split_brg_unref(g);
	core->metaif->destroy(&t->meta);
	ffmem_free(t->conf.ofile.name);  t->conf.ofile.name = NULL;
}

static int split_brg_process(void *f, phi_track *t)
{
	struct split_brg *g = f;

	if (g->state != S_PENDING) {
		split_brg_consume(g);
		if (t->chain_flags & PHI_FSTOP)
			return PHI_DONE;
		return PHI_ASYNC;
	}
	g->state = S_LOCKED;

	t->data_out = g->data;
	return PHI_DATA;
}

static const phi_filter phi_split_brg = {
	split_brg_open, split_brg_close, split_brg_process,
	"split-brg"
};


struct split {
	phi_track*	out_trk;
	struct split_brg *brg;
	uint64	split_by;
	uint64	next_split;
	uint64	total;
	ffstr	qdata;
	uint	sample_size;
	uint	split_next :1;
};

static void* split_open(phi_track *t)
{
	if (t->conf.split_msec == 0)
		return PHI_OPEN_SKIP;

	struct split *c = phi_track_allocT(t, struct split);
	c->split_by = msec_to_samples(t->conf.split_msec, t->audio.format.rate);
	c->sample_size = pcm_size(t->audio.format.format, t->audio.format.channels);
	c->split_next = 1;
	return c;
}

static void split_close(void *ctx, phi_track *t)
{
	struct split *c = ctx;
	if (c->out_trk) {
		core->track->stop(c->out_trk);
		c->brg->parent = NULL;
		split_brg_unref(c->brg);
	}
	phi_track_free(t, c);
}

/** Create a subtrack that will encode audio (supplied by us) and write the output to a file. */
static void split_next(struct split *c, phi_track *t)
{
	const phi_track_if *track = core->track;

	if (c->out_trk) {
		track->stop(c->out_trk);
		c->out_trk = NULL;
		split_brg_unref(c->brg);
		c->brg = NULL;
	}

	struct phi_track_conf conf = {
		.encoder = t->conf.encoder,
		.ofile = {
			.name = ffsz_dup(t->conf.ofile.name),
			.overwrite = t->conf.ofile.overwrite,
		},
		.stream_copy = t->conf.stream_copy,
	};
	c->out_trk = track->create(&conf);
	phi_track *ot = c->out_trk;
	core->metaif->copy(&ot->meta, &t->meta, 0);

	ot->data_type = "pcm";
	ot->audio.format = t->audio.format;
	ot->oaudio.format = t->oaudio.format;

	if (!track->filter(ot, &phi_split_brg, 0)
		|| !track->filter(ot, core->mod("afilter.auto-conv"), 0)
		|| !track->filter(ot, core->mod("format.auto-write"), 0)
		|| !track->filter(ot, core->mod("core.file-write"), 0))
		return;

	c->brg = ffmem_new(struct split_brg);
	c->brg->users = 2;
	c->brg->parent = t;
	ot->udata = c->brg;

	track->start(c->out_trk);
}

/**
We wake the current subtrack every time we have some new audio data.
The subtrack wakes us when it has finished reading our audio data. */
static int split_process(void *ctx, phi_track *t)
{
	struct split *c = ctx;

	ffstr input = c->qdata;
	c->qdata.len = 0;
	if (!input.len) {
		input = t->data_in;
		t->data_in.len = 0;
	}

	if (c->brg && split_brg_busy(c->brg))
		return PHI_ASYNC; // we're waiting for the subtrack to finish reading the data

	if (!input.len) {
		if (t->chain_flags & PHI_FFIRST)
			return PHI_DONE;
		return PHI_MORE;
	}

	uint64 pos = t->audio.pos;
	if (t->audio.pos == ~0ULL)
		pos = c->total / c->sample_size;

	if (c->split_next) {
		c->split_next = 0;
		c->next_split += c->split_by;
		split_next(c, t);
	}

	t->data_out = input;

	if (t->conf.stream_copy) {
		if (pos >= c->next_split) {
			dbglog(t, "reached block with sample #%U", c->next_split);
			c->split_next = 1;
		}

	} else {
		uint samples = input.len / c->sample_size;
		if (pos + samples >= c->next_split) {
			t->data_out.len = (c->next_split > pos) ? (c->next_split - pos) * c->sample_size : 0;

			c->qdata = input;
			ffstr_shift(&c->qdata, t->data_out.len);

			dbglog(t, "reached sample #%U", c->next_split);
			c->split_next = 1;
		}
	}

	c->total += t->data_out.len;
	split_brg_write(c->brg, t->data_out);
	core->track->wake(c->out_trk);
	return PHI_ASYNC;
}

const phi_filter phi_split = {
	split_open, split_close, split_process,
	"split"
};
