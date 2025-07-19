/** phiola: executor: 'list heal' command
2023, Simon Zolin */

static int lh_help()
{
	help_info_write("\
Correct the file paths inside playlist\n\
\n\
    `phiola list heal` [M3U...]\n\
\n\
Replace absolute file paths to relative paths, e.g.:\n\
    for /path/list.m3u:\n\
    /path/dir/file.mp3 -> dir/file.mp3\n\
\n\
Correct the file directory & extension, e.g.:\n\
    olddir/file.mp3 -> newdir/file.m4a\n\
\n\
Note: can NOT detect file renamings.\n\
");
	x->exit_code = 0;
	return 1;
}

#include <ffsys/dirscan.h>
#include <ffbase/fntree.h>
#include <ffsys/path.h>
#include <ffbase/map.h>

struct list_heal {
	ffvec	input; // char*[]

	ffvec	tasks; // fftask[]
	uint	counter;

	ffvec		pl_dir;
	ffvec		buf;
	ffmap		map; // "filename" -> struct lh_map_ent{ "/path/filename.ext" }
	ffdirscan	ds;
	ffvec		ds_path;
};

static int lh_input(struct list_heal *lh, const char *fn)
{
	*ffvec_pushT(&lh->input, const char*) = fn;
	return 0;
}

/** Return absolute & normalized file path */
static char* lh_abs_norm(ffvec *buf, ffstr base, ffstr path)
{
	buf->len = 0;

	if (!ffpath_abs(path.ptr, path.len)) {
		ffvec_addfmt(buf, "%S/%S", &base, &path);
		path = *(ffstr*)buf;
	} else {
		ffvec_realloc(buf, path.len+1, 1);
	}

	int r = ffpath_normalize(buf->ptr, buf->cap, path.ptr, path.len
		, FFPATH_SIMPLE | FFPATH_SLASH_BACKSLASH | FFPATH_FORCE_SLASH);
	FF_ASSERT(r >= 0);
	buf->len = r;
	char *afn = buf->ptr;
	afn[r] = '\0';
	return afn;
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
		dbglog("%S: couldn't find similar file in '%S'"
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

	ffstr dir, name;
	ffpath_splitpath_str(fn, &dir, &name);
	ffpath_splitname_str(name, &name, NULL);

	const struct lh_map_ent *me = ffmap_find(&lh->map, name.ptr, name.len, NULL);
	if (!me)
		return -1;

	ffvec_addstr(output, &me->path);
	return 0;
}

/** Apply filters to normalize a file name.
Return newly allocated file name if corrected;
	NULL otherwise */
static char* lh_heal(struct list_heal *lh, const char *name)
{
	int fixed = 0;
	ffvec output = {};
	ffstr path = FFSTR_INITZ(name);
	char *afn = lh_abs_norm(&lh->buf, *(ffstr*)&lh->pl_dir, path);

	if (!fffile_exists(afn)) {
		if (lh_fix_ext(lh, *(ffstr*)&lh->buf, &output)) {
			if (lh_fix_dir(lh, *(ffstr*)&lh->buf, &output)) {
				warnlog("%s: file doesn't exist and wasn't found in %S"
					, afn, &lh->pl_dir);
				goto end;
			}
		}
		dbglog("%s: file found at %s", afn, output.ptr);
		fixed = 1;
	} else {
		output = lh->buf;
		ffvec_null(&lh->buf);
	}

	if ((fixed || ffpath_abs(path.ptr, path.len))
		&& path_isparent(*(ffstr*)&lh->pl_dir, *(ffstr*)&output)) {

		dbglog("%S: converting the path to relative", &output);
		ffslice_rm((ffslice*)&output, 0, lh->pl_dir.len + 1, 1);
		fixed = 1;
	}

	if (fixed)
		dbglog("%S -> %S", &path, &output);

end:
	if (!fixed) {
		ffvec_free(&output);
		return NULL;
	}
	ffvec_addchar(&output, '\0');
	return output.ptr;
}

static void lh_save_complete(void *param, phi_track *t)
{
	struct list_heal *lh = param;
	if (ffint_fetch_add(&lh->counter, -1) - 1)
		return;

	x->exit_code = t->error & 0xff;
	x->core->sig(PHI_CORE_STOP);
}

/** Get absolute directory for the input playlist file */
static int abs_dir(ffstr fn, ffvec *buf)
{
	ffstr dir;
	ffpath_splitpath_str(fn, &dir, NULL);
	ffvec_set2(buf, &dir);
	if (!ffpath_abs(dir.ptr, dir.len)) {
		ffvec_alloc(buf, 4*1024, 1);
		if (ffps_curdir(buf->ptr, buf->cap)) {
			syserrlog("ffps_curdir");
			return -1;
		}
		buf->len = ffsz_len(buf->ptr);
		ffvec_addfmt(buf, "/%S", &dir);
		int r = ffpath_normalize(buf->ptr, buf->cap, buf->ptr, buf->len, 0);
		FF_ASSERT(r >= 0);
		buf->len = r;
	}
	return 0;
}

static void lh_ready(void *param)
{
	struct list_heal *lh = x->subcmd.obj;
	phi_queue_id q = param;
	const char *lname = x->queue->conf(q)->name;

	if (abs_dir(FFSTR_Z(lname), &lh->pl_dir))
		return;

	uint nfixed = 0, ntotal = 0;
	struct phi_queue_entry *qe;
	for (uint i = 0;  (qe = x->queue->at(q, i));  i++) {
		char *new_name;
		if ((new_name = lh_heal(lh, qe->url))) {
			qe->url = new_name;
			nfixed++;
		}
		ntotal++;
	}

	ffvec_free(&lh->tasks);
	ffvec_free(&lh->buf);
	ffvec_free(&lh->pl_dir);
	ffvec_free(&lh->ds_path);
	ffdirscan_close(&lh->ds);
	lh_free_table(lh);

	infolog("%s: corrected %u/%u paths", lname, nfixed, ntotal);
	x->queue->save(q, lname, lh_save_complete, lh);
}

static int lh_action(struct list_heal *lh)
{
	int i = 0;
	ffvec_zallocT(&lh->tasks, lh->input.len, phi_task);
	lh->counter = lh->input.len;

	char **it;
	FFSLICE_WALK(&lh->input, it) {
		struct phi_queue_conf qc = {
			.name = ffsz_dup(*it),
		};
		phi_queue_id q = x->queue->create(&qc);

		struct phi_queue_entry qe = {
			.url = *it,
		};
		x->queue->add(q, &qe);

		x->core->task(0, ffslice_itemT(&lh->tasks, i, phi_task), lh_ready, q);
		i++;
	}
	ffvec_free(&lh->input);
	return 0;
}

static int lh_fin(struct list_heal *lh)
{
	return 0;
}

#define O(m)  (void*)FF_OFF(struct list_heal, m)
static const struct ffarg lh_args[] = {
	{ "-help",		'1',	lh_help },
	{ "\0\1",		's',	lh_input },
	{ "",			'1',	lh_fin }
};
#undef O

static void lh_free(struct list_heal *lh)
{
	ffmem_free(lh);
}

struct ffarg_ctx list_heal_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct list_heal), lh_free, lh_action, lh_args);
}
