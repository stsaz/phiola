/** phiola: .m3u write
2021, Simon Zolin */

#include <avpack/m3u.h>

struct m3uw {
	m3uwrite m3;
	void *q;
	uint pos;
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
	return m;
}

static void m3uw_close(void *ctx, phi_track *t)
{
	struct m3uw *m = ctx;
	m3uwrite_close(&m->m3);
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
