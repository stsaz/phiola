/** phiola: Expand a directory
2019, Simon Zolin */

#include <track.h>
#include <ffsys/dirscan.h>
#include <ffbase/fntree.h>

extern const phi_core *core;
extern const phi_queue_if phi_queueif;
#define syswarnlog(t, ...)  phi_syswarnlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)

/**
'include' filter matches files only.
'exclude' filter matches files & directories.
Return TRUE if filename matches user's filename wildcards. */
static ffbool file_matches(phi_track *t, const char *fn, ffbool dir)
{
	ffsize fnlen = ffsz_len(fn);
	const ffstr *it;
	ffbool ok = 1;

	if (!dir) {
		ok = (t->conf.ifile.include.len == 0);
		FFSLICE_WALK(&t->conf.ifile.include, it) {
			if (0 == ffs_wildcard(it->ptr, it->len, fn, fnlen, FFS_WC_ICASE)) {
				ok = 1;
				break;
			}
		}
		if (!ok)
			return 0;
	}

	FFSLICE_WALK(&t->conf.ifile.exclude, it) {
		if (0 == ffs_wildcard(it->ptr, it->len, fn, fnlen, FFS_WC_ICASE)) {
			ok = 0;
			break;
		}
	}
	return ok;
}

struct dir_r {
	struct phi_queue_entry qe[8];
	uint num_qe, dir_removed;
	void *qcur;

	phi_track *trk;
	fntree_block *root, *blk;
	fntree_cursor cur;
};

static void dir_r_commit(struct dir_r *d)
{
	d->qcur = phi_queueif.insert_bulk(d->qcur, d->qe, d->num_qe, NULL);

	for (uint i = 0;  i < d->num_qe;  i++) {
		ffmem_free(d->qe[i].url);
		d->qe[i].url = NULL;
	}

	if (!d->dir_removed) {
		d->dir_removed = 1;
		phi_queueif.remove(d->trk->qent);
	}
}

/** Add files from the directory into the queue; add directories into a file tree block. */
static uint dir_read(struct dir_r *d, fntree_block **blk)
{
	uint n = 0;
	ffdirscan ds = {};
	char *fpath = NULL;

	ffstr path = fntree_path(*blk);
	if (ffdirscan_open(&ds, path.ptr, 0)) {
		syswarnlog(d->trk, "ffdirscan_open: %s", path.ptr);
		goto end;
	}

	const char *name;
	while ((name = ffdirscan_next(&ds))) {
		ffmem_free(fpath);
		fpath = ffsz_allocfmt("%S%c%s", &path, FFPATH_SLASH, name);

		fffileinfo fi;
		if (fffile_info_path(fpath, &fi)) {
			syswarnlog(d->trk, "fffile_info_path: %s", fpath);
			continue;
		}
		uint dir = fffile_isdir(fffileinfo_attr(&fi));

		if (!file_matches(d->trk, fpath, dir))
			continue;

		if (dir) {
			fntree_entry *e;
			if (!(e = fntree_addz(blk, name, 0))) {
				warnlog(d->trk, "fntree_addz: %s", fpath);
				continue;
			}
			fntree_block *nb = fntree_create(FFSTR_Z(fpath));
			fntree_attach(e, nb);
			continue;
		}

		d->qe[d->num_qe].url = fpath;
		fpath = NULL;

		if (++d->num_qe == FF_COUNT(d->qe)) {
			dir_r_commit(d);
			n += d->num_qe;
			d->num_qe = 0;
		}
	}

end:
	dir_r_commit(d);
	n += d->num_qe;
	d->num_qe = 0;
	ffmem_free(fpath);
	ffdirscan_close(&ds);
	return n;
}

static void* dir_open(phi_track *t)
{
	struct dir_r *d = phi_track_allocT(t, struct dir_r);
	d->qcur = t->qent;
	d->root = fntree_create(FFSTR_Z(t->conf.ifile.name));
	d->blk = d->root;
	d->trk = t;
	return d;
}

static void dir_close(struct dir_r *d, phi_track *t)
{
	for (uint i = 0;  i < d->num_qe;  i++) {
		ffmem_free(d->qe[i].url);
	}
	fntree_free_all(d->root);
	phi_track_free(d->trk, d);
}

static int dir_process(struct dir_r *d, phi_track *t)
{
	uint n = 0;
	while (n < 8) {
		n += dir_read(d, &d->blk);
		if (d->cur.cur)
			((fntree_entry*)d->cur.cur)->children = d->blk;

		fntree_entry *e;
		if (!(e = fntree_cur_next_r(&d->cur, &d->blk)))
			return PHI_FIN;
		d->blk = e->children;
	}

	core->track->wake(t);
	return PHI_ASYNC;
}

const phi_filter phi_dir_r = {
	dir_open, (void*)dir_close, (void*)dir_process,
	"dir-read"
};
