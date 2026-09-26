/** phiola: playlist functions
2023, Simon Zolin */

#include <track.h>

extern const struct phi_core *core;
#define syserrlog(...)  phi_syserrlog(core, NULL, NULL, __VA_ARGS__)
#define errlog(...)  phi_errlog(core, NULL, NULL, __VA_ARGS__)
#define syswarnlog(...)  phi_syswarnlog(core, NULL, NULL, __VA_ARGS__)
#define warnlog(...)  phi_warnlog(core, NULL, NULL, __VA_ARGS__)
#define infolog(...)  phi_infolog(core, NULL, NULL, __VA_ARGS__)
#define dbglog(...)  phi_dbglog(core, NULL, NULL, __VA_ARGS__)

struct list_ctx {
	const phi_queue_if *queue;
	const char *fn;
	phi_task task;
	phi_queue_id q;

	phi_playlist_cb on_complete;
	void *udata;
};

static struct list_ctx* list_ctx_new(const char *fn, const struct phi_playlist_conf *c, uint size)
{
	struct list_ctx *lx = ffmem_calloc(1, (size) ? size : sizeof(struct list_ctx));
	lx->queue = core->mod("core.queue");
	lx->on_complete = c->on_complete;
	lx->udata = c->udata;
	lx->fn = fn;
	return lx;
}

static void lx_done(void *p, phi_track *t)
{
	struct list_ctx *lx = p;
	// queue close
	lx->on_complete(lx->udata, t->error & 0xff);
	ffmem_free(lx);
}


static void lc_filled(struct list_ctx *lx)
{
	lx->queue->remove_multi(NULL, PHI_Q_RM_NONUNIQ);
	lx->queue->save(NULL, lx->fn, lx_done, lx);
}

static void pl_create(const char *fn, ffslice input_sz, uint flags, const struct phi_playlist_conf *c)
{
	struct list_ctx *lx = list_ctx_new(fn, c, 0);

	struct phi_queue_conf qc = {
		.tconf = {
			.ifile.include = c->include_str,
			.ifile.exclude = c->exclude_str,
		},
	};
	lx->q = lx->queue->create(&qc);

	char **it;
	FFSLICE_WALK(&input_sz, it) {
		struct phi_queue_entry qe = {
			.url = *it,
		};
		lx->queue->add(lx->q, &qe);
	}

	// it's safe to call this immediately because 'add' task will complete before 'save'
	core->task(0, &lx->task, (void*)lc_filled, lx);
}


static void ls_ready(struct list_ctx *lx)
{
	lx->queue->sort(lx->q, 0);
	lx->queue->save(lx->q, lx->fn, lx_done, lx);
}

static void pl_sort(const char *fn, uint flags, const struct phi_playlist_conf *c)
{
	struct list_ctx *lx = list_ctx_new(fn, c, 0);

	struct phi_queue_conf qc = {};
	lx->q = lx->queue->create(&qc);

	struct phi_queue_entry qe = {
		.url = (char*)fn,
	};
	lx->queue->add(lx->q, &qe);

	core->task(0, &lx->task, (void*)ls_ready, lx);
}

#include <format/playlist-heal.h>

const phi_playlist_if pl_if = {
	pl_create,
	pl_sort,
	pl_heal,
};
