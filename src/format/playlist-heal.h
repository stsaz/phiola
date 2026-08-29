/** phiola: playlist heal
2023, Simon Zolin */

#include <util/util.h>
#include <ffsys/dirscan.h>
#include <ffsys/path.h>
#include <ffsys/process.h>
#include <ffbase/fntree.h>
#include <ffbase/map.h>

struct list_heal {
	struct list_ctx lx;

	ffvec		pl_dir, ps_dir;
	ffvec		buf;
	ffmap		map; // "filename" -> struct lh_map_ent{ "/path/filename.ext" }
	ffdirscan	ds;
	ffvec		ds_path;
};

/** Return absolute & normalized file path; rebase the input path */
static ffstr lh_abs_norm(ffvec *buf, ffstr base, ffstr new_base, ffstr *ipath)
{
	ffstr path = *ipath;
	buf->len = 0;

	if (!ffpath_abs(path.ptr, path.len)) {
		ffvec_addfmt(buf, "%S/%S", &base, &path);
		path = *(ffstr*)buf;
		ffstr_shift(ipath, new_base.len - base.len);
	} else {
		ffvec_realloc(buf, path.len+1, 1);
	}

	int r = ffpath_normalize(buf->ptr, buf->cap, path.ptr, path.len
		, FFPATH_SLASH_BACKSLASH | FFPATH_FORCE_SLASH);
	FF_ASSERT(r >= 0);
	buf->len = r;
	((char*)buf->ptr)[r] = '\0';
	return *(ffstr*)buf;
}

/** Find an existing file with the same name but different extension.
/path/dir/file.mp3 -> /path/dir/file.m4a */
static int lh_fix_ext(struct list_heal *lh, ffstr fn, ffvec *output)
{
	int rc = -1;

	ffstr dir, name;
	ffpath_splitpath_str(fn, &dir, &name);
	ffpath_splitname_str(name, &name, NULL);

	if (!ffstr_eq2(&dir, &lh->ds_path)) {
		lh->ds_path.len = 0;
		ffvec_addfmt(&lh->ds_path, "%S%Z", &dir);
		lh->ds_path.len--;
		ffdirscan_close(&lh->ds);
		ffmem_zero_obj(&lh->ds);
		if (ffdirscan_open(&lh->ds, lh->ds_path.ptr, 0))
			goto end;
	} else {
		dbglog("using ffdirscan object from cache");
		ffdirscan_reset(&lh->ds);
	}

	const char *s;
	while ((s = ffdirscan_next(&lh->ds))) {
		ffstr ss = FFSTR_INITZ(s);
		if (ffstr_match2(&ss, &name) && s[name.len] == '.')
			break;
	}
	if (!s)
		goto end;

	output->len = 0;
	ffvec_addfmt(output, "%S/%s", &dir, s);
	rc = 0;

end:
	if (rc) {
		dbglog("%S: couldn't find similar file in \"%S\""
			, &fn, &dir);
		output->len = 0;
	}
	return rc;
}

struct lh_map_ent {
	ffstr name, path;
};

static int lh_map_keyeq_func(void *opaque, const void *key, ffsize keylen, void *val)
{
	struct lh_map_ent *me = val;
	return ffstr_eq(&me->name, key, keylen);
}

/** Create a table containing all file paths inside the playlist's directory */
static void lh_create_table(struct list_heal *lh)
{
	ffdirscan ds = {};
	fntree_block *root = NULL;
	fffd f = FFFILE_NULL;
	char *fpath = NULL;
	char *pl_dirz = ffsz_dupstr((ffstr*)&lh->pl_dir);

	ffmap_init(&lh->map, lh_map_keyeq_func);

	dbglog("scanning %s", pl_dirz);
	if (ffdirscan_open(&ds, pl_dirz, 0))
		goto end;

	ffstr path = FFSTR_INITZ(pl_dirz);
	if (!(root = fntree_from_dirscan(path, &ds, 0)))
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

		fffile_close(f);
		if (FFFILE_NULL == (f = fffile_open(fpath, FFFILE_READONLY)))
			continue;

		fffileinfo fi;
		if (fffile_info(f, &fi))
			continue;

		if (fffile_isdir(fffileinfo_attr(&fi))) {

			ffmem_zero_obj(&ds);

			uint dsflags = 0;
#ifdef FF_LINUX
			dsflags = FFDIRSCAN_USEFD;
			ds.fd = f;
			f = FFFILE_NULL;
#endif

			dbglog("scanning %s", path.ptr);
			if (ffdirscan_open(&ds, path.ptr, dsflags))
				continue;

			ffstr_setz(&path, fpath);
			if (!(blk = fntree_from_dirscan(path, &ds, 0)))
				continue;
			ffdirscan_close(&ds);

			fntree_attach(e, blk);
			continue;
		}

		struct lh_map_ent *me = ffmem_new(struct lh_map_ent);
		ffstr_setz(&me->path, fpath);
		fpath = NULL;
		ffpath_splitpath_str(me->path, NULL, &me->name);
		ffpath_splitname_str(me->name, &me->name, NULL);
		ffmap_add(&lh->map, me->name.ptr, me->name.len, me);
	}

end:
	ffmem_free(pl_dirz);
	ffmem_free(fpath);
	fffile_close(f);
	ffdirscan_close(&ds);
	fntree_free_all(root);
}

static void lh_free_table(struct list_heal *lh)
{
	struct _ffmap_item *it;
	FFMAP_WALK(&lh->map, it) {
		if (!_ffmap_item_occupied(it))
			continue;
		struct lh_map_ent *me = it->val;
		ffmem_free(me->path.ptr);
		ffmem_free(me);
	}
	ffmap_free(&lh->map);
}

/** Find an existing file with the same name (and probably different extension)
 recursively in playlist's directory.
/path/dir/olddir/file.mp3 -> /path/dir/newdir/file.m4a */
static int lh_fix_dir(struct list_heal *lh, ffstr fn, ffvec *output)
{
	if (!lh->map.len)
		lh_create_table(lh);

	ffstr name;
	ffpath_splitpath_str(fn, NULL, &name);
	ffpath_splitname_str(name, &name, NULL);

	const struct lh_map_ent *me = ffmap_find(&lh->map, name.ptr, name.len, NULL);
	if (!me)
		return -1;

	ffvec_addstr(output, &me->path);
	return 0;
}

/** Normalize the file path and correct the file name within a playlist.
name: file path (relative to the process current directory)
Return newly allocated file name (relative to the playlist), if corrected;
	NULL otherwise */
static char* lh_heal(struct list_heal *lh, const char *name)
{
	if (url_checkz(name))
		return NULL;

	ffvec output = {};
	ffstr pl_dir = *(ffstr*)&lh->pl_dir;
	ffstr ps_dir = *(ffstr*)&lh->ps_dir;
	ffstr ipath = FFSTR_INITZ(name);
	ffstr path_norm = lh_abs_norm(&lh->buf, ps_dir, pl_dir, &ipath);

	if (!path_isparent(pl_dir, path_norm)) {
		// File is out of scope.  Normalize the original path.
		ffvec_alloc(&output, ipath.len + 1, 1);
		int r = ffpath_normalize(output.ptr, output.cap, ipath.ptr, ipath.len
			, FFPATH_SLASH_BACKSLASH | FFPATH_FORCE_SLASH);
		FF_ASSERT(r >= 0);
		output.len = r;
		((char*)output.ptr)[r] = '\0';
		goto done;
	}

	if (!fffile_exists(path_norm.ptr)) {
		if (lh_fix_ext(lh, path_norm, &output)) {
			if (lh_fix_dir(lh, path_norm, &output)) {
				warnlog("%S: file doesn't exist and wasn't found in %S"
					, &ipath, &pl_dir);
				goto done;
			}
		}
		ffslice_rm((ffslice*)&output, 0, pl_dir.len + 1, 1);

	} else {
		// Absolute -> relative:
		// for "/path/list": "/path/dir/file" -> "dir/file"
		ffstr rel = path_norm;
		ffstr_shift(&rel, pl_dir.len + 1);
		ffvec_addstr(&output, &rel);
	}

	ffvec_addchar(&output, '\0');  output.len--;

done:
	if (ffstr_eq2(&output, &ipath)) {
		// Path unchanged
		ffvec_free(&output);
		return NULL;
	}
	dbglog("\"%S\" -> \"%S\"", &ipath, &output);
	return output.ptr;
}

static void lh_save_complete(void *param, phi_track *t)
{
	struct list_heal *lh = param;
	ffvec_free(&lh->buf);
	ffvec_free(&lh->pl_dir);
	ffvec_free(&lh->ps_dir);
	ffvec_free(&lh->ds_path);
	ffdirscan_close(&lh->ds);
	lh_free_table(lh);

	lx_done(param, t);
}

/** Get absolute directory of the playlist file */
static int abs_dir(ffstr fn, ffvec *buf, ffstr ps_dir)
{
	ffstr dir;
	ffpath_splitpath_str(fn, &dir, NULL);
	if (!ffpath_abs(dir.ptr, dir.len)) {
		ffvec_addfmt(buf, "%S/%S", &ps_dir, &dir);
		int r = ffpath_normalize(buf->ptr, buf->cap, buf->ptr, buf->len, 0);
		FF_ASSERT(r >= 0);
		buf->len = r;
	} else {
		ffvec_set2(buf, &dir);
	}
	return 0;
}

static void lh_ready(void *param)
{
	struct list_heal *lh = param;
	const char *lname = lh->lx.fn;

	ffvec_alloc(&lh->ps_dir, 4*1024, 1);
	if (ffps_curdir(lh->ps_dir.ptr, lh->ps_dir.cap)) {
		syserrlog("ffps_curdir");
		return;
	}
	lh->ps_dir.len = ffsz_len(lh->ps_dir.ptr);

	if (abs_dir(FFSTR_Z(lname), &lh->pl_dir, *(ffstr*)&lh->ps_dir))
		return;

	uint nfixed = 0, ntotal = 0;
	struct phi_queue_entry *qe;
	for (uint i = 0;  (qe = lh->lx.queue->at(lh->lx.q, i));  i++) {
		char *new_name;
		if ((new_name = lh_heal(lh, qe->url))) {
			qe->url = new_name;
			nfixed++;
		}
		ntotal++;
	}

	infolog("%s: corrected %u/%u paths", lname, nfixed, ntotal);
	lh->lx.queue->save(lh->lx.q, lname, lh_save_complete, lh);
}

void pl_heal(const char *fn, uint flags, const struct phi_playlist_conf *c)
{
	struct list_heal *lh = (void*)list_ctx_new(fn, c, sizeof(struct list_heal));

	struct phi_queue_conf qc = {};
	lh->lx.q = lh->lx.queue->create(&qc);

	struct phi_queue_entry qe = {
		.url = (void*)fn,
	};
	lh->lx.queue->add(lh->lx.q, &qe);

	core->task(0, &lh->lx.task, (void*)lh_ready, lh);
}
