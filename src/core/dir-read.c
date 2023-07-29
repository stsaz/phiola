/** phiola: Expand a directory
2019, Simon Zolin */

#include <track.h>
#include <util/fntree.h>
#include <FFOS/dirscan.h>

extern const phi_core *core;
extern const phi_queue_if phi_queueif;
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
	fffd f = FFFILE_NULL;
	char *fpath = NULL;
	int rc = -1;
	int dir_removed = 0;

	if (0 != ffdirscan_open(&ds, fn, 0))
		goto end;

	if (NULL == (root = fntree_from_dirscan(FFSTR_Z(fn), &ds, 0)))
		goto end;
	ffdirscan_close(&ds);

	fntree_block *blk = root;
	fntree_cursor cur = {};
	for (;;) {
		fntree_entry *e;
		if (NULL == (e = fntree_cur_next_r_ctx(&cur, &blk)))
			break;

		ffstr path = fntree_path(blk);
		ffstr name = fntree_name(e);
		ffmem_free(fpath);
		fpath = ffsz_allocfmt("%S/%S", &path, &name);

		fffile_close(f);
		if (FFFILE_NULL == (f = fffile_open(fpath, FFFILE_READONLY))) {
			syswarnlog(t, "file open: %s", fpath);
			continue;
		}

		fffileinfo fi;
		if (0 != fffile_info(f, &fi))
			continue;

		if (!file_matches(t, fpath, fffile_isdir(fffileinfo_attr(&fi))))
			continue;

		if (fffile_isdir(fffileinfo_attr(&fi))) {

			ffmem_zero_obj(&ds);
			uint flags = 0;
#ifdef FF_WIN
			fffile_close(f);  f = FFFILE_NULL;
#else
			flags = FFDIRSCAN_USEFD;
			ds.fd = f;
			f = FFFILE_NULL;
#endif
			if (0 != ffdirscan_open(&ds, fpath, flags))
				continue;

			ffstr_setz(&path, fpath);
			if (NULL == (blk = fntree_from_dirscan(path, &ds, 0)))
				continue;
			ffdirscan_close(&ds);

			fntree_attach(e, blk);
			continue;
		}

		struct phi_queue_entry qe = {};
		phi_track_conf_assign(&qe.conf, &t->conf);
		qe.conf.ifile.name = fpath;
		fpath = NULL;

		qcur = phi_queueif.insert(qcur, &qe);

		if (!dir_removed) {
			dir_removed = 1;
			phi_queueif.remove(t->qent);
		}
	}

	rc = 0;

end:
	ffmem_free(fpath);
	fffile_close(f);
	ffdirscan_close(&ds);
	fntree_free_all(root);
	return rc;
}

static void* dir_open(phi_track *t)
{
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
