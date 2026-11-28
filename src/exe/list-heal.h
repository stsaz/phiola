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

struct list_heal {
	ffvec	input; // char*[]

	const phi_playlist_if *pl_if;
	phi_queue_id q;
	phi_task task;
	uint counter;
	int result;
};

static void lh_action2(struct list_heal *lh);

static int lh_input(struct list_heal *lh, const char *fn)
{
	*ffvec_pushT(&lh->input, const char*) = fn;
	return 0;
}

static void lh_saved(void *param, phi_track *t)
{
	struct list_heal *lh = param;
	lh->result |= t->error & 0xff;
	lh->counter++;
	if (lh->counter < lh->input.len) {
		x->core->task(0, &lh->task, (phi_task_func)lh_action2, lh);
		return;
	}

	x->exit_code = lh->result;
	x->core->sig(PHI_CORE_STOP);
}

static void lh_ready(struct list_heal *lh)
{
	x->subcmd.q_expanded = NULL;
	const char *fn = *ffslice_itemT(&lh->input, lh->counter, char*);

	if (!lh->pl_if) {
		if (!(lh->pl_if = x->core->mod("format.playlist")))
			goto err;
	}
	lh->pl_if->heal(lh->q, fn);
	if (x->queue->save(lh->q, fn, lh_saved, lh))
		goto err;
	return;

err:
	x->exit_code = 1;
	x->core->sig(PHI_CORE_STOP);
}

static void lh_action2(struct list_heal *lh)
{
	struct phi_queue_conf qc = {};
	lh->q = x->queue->create(&qc);

	struct phi_queue_entry qe = {
		.url = *ffslice_itemT(&lh->input, lh->counter, char*),
	};
	x->queue->add(lh->q, &qe);

	if (!x->queue->status(lh->q))
		lh_ready(lh);
	else
		x->subcmd.q_expanded = (phi_task_func)lh_ready;
}

static int lh_action(struct list_heal *lh)
{
	x->queue->on_change(q_on_change);
	lh_action2(lh);
	return 0;
}

#define O(m)  (void*)FF_OFF(struct list_heal, m)
static const struct ffarg lh_args[] = {
	{ "-help",		'1',	lh_help },
	{ "\0\1",		's',	lh_input },
	{ "",			0,		NULL }
};
#undef O

static void lh_free(struct list_heal *lh)
{
	ffvec_free(&lh->input);
	ffmem_free(lh);
}

struct ffarg_ctx list_heal_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct list_heal), lh_free, lh_action, lh_args);
}
