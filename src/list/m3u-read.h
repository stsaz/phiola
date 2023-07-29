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
	if (!metaif)
		metaif = core->mod("format.meta");

	struct m3u *m = ffmem_new(struct m3u);
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
	ffmem_free(m);
}

static int m3u_add(struct m3u *m, phi_track *t)
{
	ffstr url;

	if (0 != plist_fullname(t, *(ffstr*)&m->pls_ent.url, &url))
		return 1;

	uint dur = 0;
	if (m->pls_ent.duration != -1)
		dur = m->pls_ent.duration * 1000;

	struct phi_queue_entry qe = {
		.length_msec = dur,
	};
	phi_track_conf_assign(&qe.conf, &t->conf);
	qe.conf.ifile.name = url.ptr;
	ffstr_null(&url);
	if (t->conf.ofile.name)
		qe.conf.ofile.name = ffsz_dup(t->conf.ofile.name);
	metaif->copy(&qe.conf.meta, &t->conf.meta);

	if (m->pls_ent.artist.len)
		metaif->set(&qe.conf.meta, FFSTR_Z("artist"), *(ffstr*)&m->pls_ent.artist);

	if (m->pls_ent.title.len)
		metaif->set(&qe.conf.meta, FFSTR_Z("title"), *(ffstr*)&m->pls_ent.title);

	m->qu_cur = queue->insert(m->qu_cur, &qe);

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

		switch (r) {
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
			ffstr_set2(&m->pls_ent.url, &val);
			if (0 != m3u_add(m, t))
				return PHI_ERR;
			break;

		case M3UREAD_WARN:
			warnlog(t, "parse error: %s", m3uread_error(&m->m3u));
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
