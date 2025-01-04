/** phiola: Expand a directory
2019, Simon Zolin */

#include <track.h>
#include <ffsys/dirscan.h>
#include <ffbase/fntree.h>

extern const phi_core *core;
extern const phi_queue_if phi_queueif;
extern const phi_meta_if *phi_metaif;
#define syswarnlog(t, ...)  phi_syswarnlog(core, NULL, t, __VA_ARGS__)

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

/** Recursively add file tree into queue */
static int qu_add_dir_r(const char *fn, phi_track *t)
{
	void *qcur = t->qent;
	ffdirscan ds = {};
	fntree_block *root = NULL;
	char *fpath = NULL;
	int rc = -1;
	int dir_removed = 0;

	if (ffdirscan_open(&ds, fn, 0))
		goto end;

	if (!(root = fntree_from_dirscan(FFSTR_Z(fn), &ds, 0)))
		goto end;
	ffdirscan_close(&ds);

	fntree_block *blk = root;
	fntree_cursor cur = {};
	for (;;) {
		fntree_entry *e;
		if (!(e = fntree_cur_next_r_ctx(&cur, &blk)))
			break;

		ffstr path = fntree_path(blk);
		ffstr name = fntree_name(e);
		ffmem_free(fpath);
		fpath = ffsz_allocfmt("%S%c%S", &path, FFPATH_SLASH, &name);

		fffileinfo fi;
		if (fffile_info_path(fpath, &fi))
			continue;
		uint isdir = fffile_isdir(fffileinfo_attr(&fi));

		if (!file_matches(t, fpath, isdir))
			continue;

		if (isdir) {
			ffmem_zero_obj(&ds);
			if (ffdirscan_open(&ds, fpath, 0))
				continue;

			ffstr_setz(&path, fpath);
			if (!(blk = fntree_from_dirscan(path, &ds, 0)))
				continue;
			ffdirscan_close(&ds);

			fntree_attach(e, blk);
			continue;
		}

		struct phi_queue_entry qe = {
			.url = fpath,
		};
		qcur = phi_queueif.insert(qcur, &qe);
		ffmem_free(fpath);
		fpath = NULL;

		if (!dir_removed) {
			dir_removed = 1;
			phi_queueif.remove(t->qent);
		}
	}

	rc = 0;

end:
	if (!dir_removed) {
		dir_removed = 1;
		phi_queueif.remove(t->qent);
	}

	ffmem_free(fpath);
	ffdirscan_close(&ds);
	fntree_free_all(root);
	return rc;
}

static void* dir_open(phi_track *t)
{
	if (!phi_metaif)
		phi_metaif = core->mod("format.meta");

	if (!!qu_add_dir_r(t->conf.ifile.name, t))
		return PHI_OPEN_ERR;
	return (void*)1;
}

static int dir_process(void *ctx, phi_track *t)
{
	return PHI_FIN;
}

const phi_filter phi_dir_r = {
	dir_open, NULL, dir_process,
	"dir-read"
};
