/** phiola: .m3u read
2021, Simon Zolin */

#include <avpack/m3u.h>

struct m3u {
	m3uread m3u;
	struct phi_queue_entry ent;
	pls_entry pls_ent;
	void *qu_cur;
	uint fin :1;
	uint m3u_removed :1;
};

static void* m3u_open(phi_track *t)
{
	if (!queue)
		queue = core->mod("core.queue");

	struct m3u *m = phi_track_allocT(t, struct m3u);
	m3uread_open(&m->m3u);
	m->qu_cur = t->qent;
	return m;
}

static void m3u_close(void *ctx, phi_track *t)
{
	struct m3u *m = ctx;
	if (!m->m3u_removed) {
		queue->remove(t->qent);
	}
	m3uread_close(&m->m3u);
	pls_entry_free(&m->pls_ent);
	phi_track_free(t, m);

	/* When auto-loading playlist at GUI startup - set it as "not modified" */
	struct phi_queue_conf *qc = queue->conf(queue->queue(t->qent));
	if (qc->last_mod_time.sec)
		qc->modified = 0;
}

static int m3u_add(struct m3u *m, phi_track *t)
{
	ffstr url;

	if (0 != plist_fullname(t, *(ffstr*)&m->pls_ent.url, &url))
		return 1;

	struct phi_queue_entry qe = {
		.url = url.ptr,
		.length_sec = (m->pls_ent.duration != -1) ? m->pls_ent.duration : 0,
	};

	if (m->pls_ent.artist.len)
		core->metaif->set(&qe.meta, FFSTR_Z("artist"), *(ffstr*)&m->pls_ent.artist, 0);

	if (m->pls_ent.title.len)
		core->metaif->set(&qe.meta, FFSTR_Z("title"), *(ffstr*)&m->pls_ent.title, 0);

	m->qu_cur = queue->insert(m->qu_cur, &qe);
	ffstr_free(&url);

	if (!m->m3u_removed) {
		m->m3u_removed = 1;
		queue->remove(t->qent);
	}

	pls_entry_free(&m->pls_ent);
	return 0;
}

/** Allocate and copy data from memory pointed by 'a.ptr'. */
#define _ffvec_copyself(a) \
do { \
	if ((a)->cap == 0 && (a)->len != 0) \
		ffvec_realloc(a, (a)->len, 1); \
} while (0)

static int m3u_process(void *ctx, phi_track *t)
{
	struct m3u *m = ctx;
	int r, fin = 0;
	ffstr data, val;

	data = t->data_in;

	for (;;) {

		r = m3uread_process(&m->m3u, &data, &val);

		switch ((enum M3UREAD_R)r) {
		case M3UREAD_MORE:
			if (!(t->chain_flags & PHI_FFIRST)) {
				_ffvec_copyself(&m->pls_ent.artist);
				_ffvec_copyself(&m->pls_ent.title);
				return PHI_MORE;
			}

			if (!fin) {
				fin = 1;
				ffstr_setcz(&data, "\n");
				continue;
			}

			return PHI_FIN;

		case M3UREAD_ARTIST:
			ffstr_set2(&m->pls_ent.artist, &val);
			break;

		case M3UREAD_TITLE:
			ffstr_set2(&m->pls_ent.title, &val);
			break;

		case M3UREAD_DURATION:
			m->pls_ent.duration = m3uread_duration_sec(&m->m3u);
			break;

		case M3UREAD_URL:
			if (!ffutf8_valid(val.ptr, val.len)) {
				warnlog(t, "incorrect UTF-8 data in URL");
				continue;
			}

			ffstr_set2(&m->pls_ent.url, &val);
			if (0 != m3u_add(m, t))
				return PHI_ERR;
			break;

		case M3UREAD_WARN:
			warnlog(t, "parse error: %s", m3uread_error(&m->m3u));
			continue;

		case M3UREAD_EXT:
			continue;

		default:
			FF_ASSERT(0);
			return PHI_ERR;
		}
	}
}

const phi_filter phi_m3u_read = {
	m3u_open, m3u_close, m3u_process,
	"m3u-read"
};
