/** phiola: .m3u write
2021, Simon Zolin */

#include <avpack/m3u.h>
#include <util/util.h>
#include <ffsys/globals.h>

struct m3uw {
	m3uwrite m3;
	void *q;
	uint pos;
	ffstr odir;
	ffvec fn;
};

static void* m3uw_open(phi_track *t)
{
	if (t->udata == NULL)
		return PHI_OPEN_SKIP;

	if (!queue)
		queue = core->mod("core.queue");

	struct m3uw *m = phi_track_allocT(t, struct m3uw);
	m->q = t->udata;
	m3uwrite_create(&m->m3, 0);
	ffpath_splitpath_str(FFSTR_Z(t->conf.ofile.name), &m->odir, NULL);
	return m;
}

static void m3uw_close(void *ctx, phi_track *t)
{
	struct m3uw *m = ctx;
	m3uwrite_close(&m->m3);
	ffvec_free(&m->fn);
	phi_track_free(t, m);
}

static int m3uw_process(void *ctx, phi_track *t)
{
	struct m3uw *m = ctx;
	struct phi_queue_entry *qe;

	while (NULL != (qe = queue->at(m->q, m->pos++))) {

		m3uwrite_entry m3e = {
			.url = FFSTR_Z(qe->url),
			.duration_sec = (qe->length_sec) ? (int)qe->length_sec : -1,
		};

		ffvec_realloc(&m->fn, m3e.url.len, 1);
		m->fn.len = ffpath_normalize(m->fn.ptr, m->fn.cap, m3e.url.ptr, m3e.url.len, 0);
		m3e.url = *(ffstr*)&m->fn;

		// "/dir/list.m3u" + "/dir/artist/title.mp3" => "artist/title.mp3"
		if (path_isparent(m->odir, m3e.url)) {
			ffstr_shift(&m3e.url, m->odir.len + 1);
		}

		core->metaif->find(&qe->meta, FFSTR_Z("artist"), &m3e.artist, 0);
		core->metaif->find(&qe->meta, FFSTR_Z("title"), &m3e.title, 0);
		m3uwrite_process(&m->m3, &m3e);
	}

	t->data_out = m3uwrite_fin(&m->m3);
	return PHI_LASTOUT;
}

const phi_filter phi_m3u_write = {
	m3uw_open, m3uw_close, m3uw_process,
	"m3u-write"
};
