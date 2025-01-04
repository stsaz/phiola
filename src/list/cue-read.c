/** phiola: .cue read
2019, Simon Zolin */

#include <track.h>
#include <list/entry.h>
#include <util/util.h>
#include <avpack/cue.h>

extern const phi_core *core;
static const phi_queue_if *queue;
static const phi_meta_if *metaif;
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)

struct ffcuetrk {
	uint from, to;
};

struct ffcue {
	uint options; // enum PHI_CUE_GAP
	uint from;
	struct ffcuetrk trk;
	uint first :1;
	uint next :1;
};

struct cue {
	cueread cue;
	struct ffcue cu;
	ffstr url;
	void *qu_cur;
	ffvec val;

	ffvec gmetas;
	ffvec metas;
	uint nmeta;

	uint curtrk;
	uint have_gmeta :1;
	uint utf8 :1;
	uint removed :1;
	uint finalizing :1;
	uint done :1;
	uint next_file :1;
};

static void cue_close(void *ctx, phi_track *t)
{
	struct cue *c = ctx;
	cueread_close(&c->cue);
	ffstr_free(&c->url);
	FFSLICE_FOREACH_T(&c->gmetas, ffvec_free, ffvec);
	FFSLICE_FOREACH_T(&c->metas, ffvec_free, ffvec);
	ffvec_free(&c->gmetas);
	ffvec_free(&c->metas);
	ffvec_free(&c->val);
	phi_track_free(t, c);
}

static void* cue_open(phi_track *t)
{
	if (!queue)
		queue = core->mod("core.queue");
	if (!metaif)
		metaif = core->mod("format.meta");

	struct cue *c = phi_track_allocT(t, struct cue);
	ffarrint32_sort((uint*)t->conf.tracks.ptr, t->conf.tracks.len);
	cueread_open(&c->cue);
	c->qu_cur = t->qent;
	c->cu.options = t->conf.cue_gaps;
	c->utf8 = 1;
	return c;
}

/** Find meta name in array.
Return -1 if not found. */
static ssize_t cue_meta_find(const ffvec *a, ffsize n, const ffvec *search)
{
	const ffvec *m = (void*)a->ptr;
	for (ffsize i = 0;  i != n;  i += 2) {
		if (ffstr_eq2(&m[i], search))
			return i;
	}
	return -1;
}

/** Get track start/end time from two INDEX values.
@type: enum CUEREAD_T.
 0xa11: get info for the final track.
Return NULL if the track duration isn't known yet. */
static struct ffcuetrk* ffcue_index(struct ffcue *c, uint type, uint val)
{
	switch (type) {
	case CUEREAD_FILE:
		c->from = -1;
		c->first = 1;
		break;

	case CUEREAD_TRK_NUM:
		c->trk.from = c->trk.to = -1;
		c->next = 0;
		break;

	case CUEREAD_TRK_INDEX00:
		if (c->first) {
			if (c->options == PHI_CUE_GAP_PREV1 || c->options == PHI_CUE_GAP_CURR)
				c->from = val;
			break;
		}

		if (c->options == PHI_CUE_GAP_SKIP)
			c->trk.to = val;
		else if (c->options == PHI_CUE_GAP_CURR) {
			c->trk.to = val;
			c->trk.from = c->from;
			c->from = val;
		}
		break;

	case CUEREAD_TRK_INDEX:
		if (c->first) {
			c->first = 0;
			if (c->from == (uint)-1)
				c->from = val;
			break;
		} else if (c->next)
			return NULL; // skip "INDEX XX" after "INDEX 01"

		if (c->trk.from == (uint)-1) {
			c->trk.from = c->from;
			c->from = val;
		}

		if (c->trk.to == (uint)-1)
			c->trk.to = val;

		c->next = 1;
		return &c->trk;

	case 0xa11:
		c->trk.from = c->from;
		c->trk.to = 0;
		return &c->trk;
	}

	return NULL;
}

/**
Return PHI_DATA when a track is ready;
	PHI_ERR, PHI_MORE */
static int cue_parse(struct cue *c, phi_track *t)
{
	ffvec *meta;
	ffstr metaname, in = t->data_in, out = {};
	int rc = PHI_ERR, r;

	if (c->next_file) {
		c->next_file = 0;
		ffmem_zero_obj(&c->cu);
		c->cu.options = t->conf.cue_gaps;
		r = CUEREAD_FILE;
		goto file;
	}

	for (;;) {
		r = cueread_process(&c->cue, &in, &out);

		switch (r) {
		case CUEREAD_MORE:
			if (!(t->chain_flags & PHI_FFIRST)) {
				rc = PHI_MORE;
				goto end;
			}

			if (c->finalizing) {
				// end of .cue file
				c->done = 1;
				ffcue_index(&c->cu, 0xa11, 0);
				c->nmeta = c->metas.len;
				rc = PHI_DATA;
				goto end;
			}

			c->finalizing = 1;
			ffstr_setcz(&in, "\n");
			continue;

		case CUEREAD_WARN:
			warnlog(t, "parse error at line %u: %s"
				, (int)cueread_line(&c->cue), cueread_error(&c->cue));
			continue;
		}

		ffstr *v = &out;
		if (c->utf8 && ffutf8_valid(v->ptr, v->len)) {
			c->val.len = 0;
			ffvec_addstr(&c->val, v);
		} else {
			c->utf8 = 0;
			ffsize n = ffutf8_from_cp(NULL, 0, v->ptr, v->len, core->conf.code_page);
			ffvec_realloc(&c->val, n, 1);
			c->val.len = ffutf8_from_cp(c->val.ptr, c->val.cap, v->ptr, v->len, core->conf.code_page);
		}

		switch (r) {
		case CUEREAD_TITLE:
			ffstr_setcz(&metaname, "album");
			goto add_metaname;

		case CUEREAD_PERFORMER:
			ffstr_setcz(&metaname, "artist");
			goto add_metaname;

		case CUEREAD_TRK_NUM:
			c->nmeta = c->metas.len;
			ffstr_setcz(&metaname, "tracknumber");
			goto add_metaname;

		case CUEREAD_TRK_TITLE:
			ffstr_setcz(&metaname, "title");
			goto add_metaname;

		case CUEREAD_TRK_PERFORMER:
			ffstr_setcz(&metaname, "artist");

		add_metaname:
			meta = ffvec_pushT(&c->metas, ffvec);
			ffstr_set2(meta, &metaname);
			meta->cap = 0;
			// fallthrough

		case CUEREAD_REM_VAL:
		case CUEREAD_REM_NAME:
			*ffvec_pushT(&c->metas, ffvec) = c->val;
			ffvec_null(&c->val);
			break;

		case CUEREAD_FILE:
			if (c->url.len) {
				// got next FILE entry -> finalize the current track
				c->next_file = 1;
				ffcue_index(&c->cu, 0xa11, 0);
				c->nmeta = c->metas.len;
				rc = PHI_DATA;
				goto end;
			}

		file:
			if (!c->have_gmeta) {
				c->have_gmeta = 1;
				c->gmetas = c->metas;
				ffvec_null(&c->metas);
			}
			ffstr_free(&c->url);
			if (plist_fullname(t, *(ffstr*)&c->val, &c->url))
				goto end;
			break;
		}

		if (ffcue_index(&c->cu, r, cueread_cdframes(&c->cue))) {
			rc = PHI_DATA;
			goto end;
		}
	}

end:
	t->data_in = in;
	return rc;
}

static void add_track(struct cue *c, struct ffcuetrk *ctrk, phi_track *t)
{
	dbglog(t, "add '%S' %d..%d", &c->url, ctrk->from, ctrk->to);
	struct phi_queue_entry qe = {
		.url = c->url.ptr,
		.seek_cdframes = ctrk->from,
		.until_cdframes = ctrk->to,
		.length_msec = (ctrk->to) ? (ctrk->to - ctrk->from) * 1000 / 75 : 0,
		.meta_priority = 1,
	};

	// add global meta that isn't set in TRACK context
	ffvec *m = (void*)c->gmetas.ptr;
	for (uint i = 0;  i != c->gmetas.len;  i += 2) {

		if (cue_meta_find(&c->metas, c->nmeta, &m[i]) >= 0)
			continue;

		metaif->set(&qe.meta, *(ffstr*)&m[i], *(ffstr*)&m[i + 1], 0);
	}

	// add TRACK meta
	m = (void*)c->metas.ptr;
	for (uint i = 0;  i != c->nmeta;  i += 2) {
		metaif->set(&qe.meta, *(ffstr*)&m[i], *(ffstr*)&m[i + 1], 0);
	}

	c->qu_cur = queue->insert(c->qu_cur, &qe);

	if (!c->removed) {
		c->removed = 1;
		queue->remove(t->qent);
	}
}

static int cue_process(void *ctx, phi_track *t)
{
	struct cue *c = ctx;
	int r;
	ffvec *m;

	while (!c->done) {

		if (PHI_DATA != (r = cue_parse(c, t)))
			return r;

		c->curtrk++;
		if (t->conf.tracks.len) {
			ffsize n = ffarrint32_binfind((uint*)t->conf.tracks.ptr, t->conf.tracks.len, c->curtrk);
			if ((ffssize)n < 0)
				goto next;
			if (n == t->conf.tracks.len - 1)
				c->done = 1; // Don't parse the rest
		}

		if (c->cu.trk.to != 0 && c->cu.trk.from >= c->cu.trk.to) {
			warnlog(t, "invalid INDEX values");
			continue;
		}

		add_track(c, &c->cu.trk, t);

	next:
		/* 'metas': TRACK_N TRACK_N+1
		Remove the items for TRACK_N. */
		m = (void*)c->metas.ptr;
		for (uint i = 0;  i != c->nmeta;  i++) {
			ffvec_free(&m[i]);
		}
		ffslice_rm((ffslice*)&c->metas, 0, c->nmeta, sizeof(ffvec));
		c->nmeta = c->metas.len;
	}

	return PHI_FIN;
}

const phi_filter phi_cue_read = {
	cue_open, cue_close, cue_process,
	"cue-read"
};


struct cuehook {
	uint64 abs_seek;
	uint64 abs_seek_ms;
};

#define cdframes_to_samples(val, rate)  ((val) * (rate) / 75)
#define cdframes_to_msec(val)  ((val) / 75 * 1000)

static void* cuehook_open(phi_track *t)
{
	if (!(t->conf.seek_cdframes || t->conf.until_cdframes))
		return PHI_OPEN_SKIP;

	struct cuehook *c = phi_track_allocT(t, struct cuehook);
	c->abs_seek = cdframes_to_samples(t->conf.seek_cdframes, t->audio.format.rate);
	c->abs_seek_ms = cdframes_to_msec(t->conf.seek_cdframes);
	dbglog(t, "abs_seek:%U (%Ums)", c->abs_seek, c->abs_seek_ms);
	t->audio.total -= c->abs_seek;
	if (t->conf.until_cdframes)
		t->conf.until_msec += cdframes_to_msec(t->conf.until_cdframes - t->conf.seek_cdframes);
	return c;
}

static void cuehook_close(void *ctx, phi_track *t)
{
	struct cuehook *c = ctx;
	phi_track_free(t, c);
}

static int cuehook_process(void *ctx, phi_track *t)
{
	struct cuehook *c = ctx;

	if (t->chain_flags & PHI_FFWD) {
		t->audio.pos = ffmax((int64)(t->audio.pos - c->abs_seek), 0);
		t->data_out = t->data_in;
		if (t->chain_flags & PHI_FFIRST)
			return PHI_DONE;
		return PHI_DATA;
	}

	if (t->audio.seek != -1) {
		t->audio.seek += c->abs_seek_ms;
		dbglog(t, "seek:%U", t->audio.seek);
	}
	return PHI_MORE;
}

const phi_filter phi_cue_hook = {
	cuehook_open, cuehook_close, cuehook_process,
	"cue-hook"
};
