/** phiola: .pls read
2021, Simon Zolin */

#include <track.h>
#include <list/entry.h>
#include <avpack/pls.h>

extern const phi_core *core;
static const phi_queue_if *queue;
static const phi_meta_if *metaif;
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)

struct pls_r {
	plsread pls;
	pls_entry pls_ent;
	void *qu_cur;
	uint removed :1;
};

static void* pls_open(phi_track *t)
{
	if (!queue)
		queue = core->mod("core.queue");
	if (!metaif)
		metaif = core->mod("format.meta");

	struct pls_r *p = ffmem_new(struct pls_r);
	plsread_open(&p->pls);
	p->qu_cur = t->qent;
	return p;
}

static void pls_close(void *ctx, phi_track *t)
{
	struct pls_r *p = ctx;
	plsread_close(&p->pls);
	pls_entry_free(&p->pls_ent);
	ffmem_free(p);
}

static int pls_add(struct pls_r *p, phi_track *t)
{
	ffstr url;

	if (0 != plist_fullname(t, *(ffstr*)&p->pls_ent.url, &url))
		return 1;

	struct phi_queue_entry qe = {
		.length_msec = (p->pls_ent.duration != -1) ? p->pls_ent.duration * 1000 : 0,
	};
	phi_track_conf_assign(&qe.conf, &t->conf);
	qe.conf.ifile.name = url.ptr;
	ffstr_null(&url);
	if (t->conf.ofile.name)
		qe.conf.ofile.name = ffsz_dup(t->conf.ofile.name);
	metaif->copy(&qe.conf.meta, &t->conf.meta);

	if (!qe.conf.meta.len && p->pls_ent.title.len) {
		metaif->set(&qe.conf.meta, FFSTR_Z("title"), *(ffstr*)&p->pls_ent.title);
		qe.conf.meta_transient = 1;
	}

	p->qu_cur = queue->insert(p->qu_cur, &qe);

	if (!p->removed) {
		p->removed = 1;
		queue->remove(t->qent);
	}

	pls_entry_free(&p->pls_ent);
	return 0;
}

/** Allocate and copy data from memory pointed by 'a.ptr'. */
#define _ffvec_copyself(a) \
do { \
	if ((a)->cap == 0 && (a)->len != 0) \
		ffvec_realloc(a, (a)->len, 1); \
} while (0)

static int pls_process(void *ctx, phi_track *t)
{
	struct pls_r *p = ctx;
	int r, fin = 0, commit = 0;
	ffstr data = t->data_in, val;
	ffuint trk_idx = (ffuint)-1, idx;

	for (;;) {

		r = plsread_process(&p->pls, &data, &val, &idx);

		switch (r) {
		case PLSREAD_MORE:
			if (!(t->chain_flags & PHI_FFIRST)) {
				_ffvec_copyself(&p->pls_ent.url);
				_ffvec_copyself(&p->pls_ent.title);
				return PHI_MORE;
			}

			if (!fin) {
				fin = 1;
				ffstr_setcz(&data, "\n");
				continue;
			}

			commit = 1;
			break;

		case PLSREAD_URL:
		case PLSREAD_TITLE:
		case PLSREAD_DURATION:
			if (trk_idx == (ffuint)-1) {
				trk_idx = idx;
			} else if (idx != trk_idx) {
				trk_idx = idx;
				commit = 1;
			}
			break;

		case PLSREAD_WARN:
			warnlog(t, "parse error: %s", plsread_error(&p->pls));
			continue;

		default:
			FF_ASSERT(0);
			return PHI_ERR;
		}

		if (commit) {
			commit = 0;
			if (p->pls_ent.url.len != 0 && 0 != pls_add(p, t))
				return PHI_ERR;
			if (fin)
				return PHI_FIN;
		}

		switch (r) {
		case PLSREAD_URL:
			ffstr_set2(&p->pls_ent.url, &val);
			break;

		case PLSREAD_TITLE:
			ffstr_set2(&p->pls_ent.title, &val);
			break;

		case PLSREAD_DURATION:
			p->pls_ent.duration = plsread_duration_sec(&p->pls);
			break;
		}
	}
}

const phi_filter phi_pls_read = {
	pls_open, pls_close, pls_process,
	"pls-read"
};
