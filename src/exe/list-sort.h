/** phiola: executor: 'list sort' command
2023, Simon Zolin */

static int ls_help()
{
	help_info_write("\
Sort entries in playlist\n\
\n\
    `phiola list sort` [M3U...]\n\
");
	x->exit_code = 0;
	return 1;
}

struct list_sort {
	ffvec	input; // char*[]

	ffvec	tasks; // phi_task[]
	uint	counter;
};

static int ls_input(struct list_sort *ls, const char *fn)
{
	*ffvec_pushT(&ls->input, const char*) = fn;
	return 0;
}

static void ls_save_complete(void *param, phi_track *t)
{
	struct list_sort *ls = param;
	if (ffint_fetch_add(&ls->counter, -1) - 1)
		return;

	x->exit_code = t->error & 0xff;
	x->core->sig(PHI_CORE_STOP);
}

static void ls_ready(void *param)
{
	phi_queue_id q = param;
	x->queue->sort(q, 0);
	x->queue->save(q, x->queue->conf(q)->name, ls_save_complete, x->subcmd.obj);
}

static int ls_action(struct list_sort *ls)
{
	int i = 0;
	ffvec_zallocT(&ls->tasks, ls->input.len, phi_task);
	ls->counter = ls->input.len;

	char **it;
	FFSLICE_WALK(&ls->input, it) {
		struct phi_queue_conf qc = {
			.name = ffsz_dup(*it),
		};
		phi_queue_id q = x->queue->create(&qc);

		struct phi_queue_entry qe = {
			.conf.ifile.name = ffsz_dup(*it),
		};
		x->queue->add(q, &qe);

		x->core->task(0, ffslice_itemT(&ls->tasks, i, phi_task), ls_ready, q);
		i++;
	}
	ffvec_free(&ls->input);
	return 0;
}

static int ls_fin(struct list_sort *ls)
{
	return 0;
}

#define O(m)  (void*)FF_OFF(struct list_sort, m)
static const struct ffarg list_sort_args[] = {
	{ "-help",		'1',	ls_help },
	{ "\0\1",		's',	ls_input },
	{ "",			'1',	ls_fin }
};
#undef O

static void list_sort_free(struct list_sort *ls)
{
	ffvec_free(&ls->input);
	ffvec_free(&ls->tasks);
	ffmem_free(ls);
}

struct ffarg_ctx list_sort_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct list_sort), list_sort_free, ls_action, list_sort_args);
}
