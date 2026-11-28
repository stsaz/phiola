/** phiola: executor: 'list sort' command
2023, Simon Zolin */

static int ls_help()
{
	help_info_write("\
Sort entries in playlist\n\
\n\
    `phiola list sort` [M3U...]\n\
Options:\n\
\n\
  `-nodups`               Remove duplicate tracks\n\
");
	x->exit_code = 0;
	return 1;
}

struct list_sort {
	ffvec	input; // char*[]
	u_char	nodups;

	phi_queue_id q;
	phi_task task;
	uint	counter;
	int		result;
};

static void ls_action2(struct list_sort *ls);

static int ls_input(struct list_sort *ls, const char *fn)
{
	*ffvec_pushT(&ls->input, const char*) = fn;
	return 0;
}

static void ls_saved(void *param, phi_track *t)
{
	struct list_sort *ls = param;
	ls->result |= t->error & 0xff;
	ls->counter++;
	if (ls->counter < ls->input.len) {
		x->core->task(0, &ls->task, (phi_task_func)ls_action2, ls);
		return;
	}

	x->exit_code = ls->result;
	x->core->sig(PHI_CORE_STOP);
}

static void ls_ready(struct list_sort *ls)
{
	x->subcmd.q_expanded = NULL;

	if (ls->nodups)
		x->queue->remove_multi(ls->q, PHI_Q_RM_DUPS);
	else
		x->queue->sort(ls->q, 0);

	const char *fn = *ffslice_itemT(&ls->input, ls->counter, char*);
	if (x->queue->save(ls->q, fn, ls_saved, ls))
		goto err;
	return;

err:
	x->exit_code = 1;
	x->core->sig(PHI_CORE_STOP);
}

static void ls_action2(struct list_sort *ls)
{
	struct phi_queue_conf qc = {};
	ls->q = x->queue->create(&qc);

	struct phi_queue_entry qe = {
		.url = *ffslice_itemT(&ls->input, ls->counter, char*),
	};
	x->queue->add(ls->q, &qe);

	if (!x->queue->status(ls->q))
		ls_ready(ls);
	else
		x->subcmd.q_expanded = (phi_task_func)ls_ready;
}

static int ls_action(struct list_sort *ls)
{
	x->queue->on_change(q_on_change);
	ls_action2(ls);
	return 0;
}

#define O(m)  (void*)FF_OFF(struct list_sort, m)
static const struct ffarg list_sort_args[] = {
	{ "-help",		'1',	ls_help },
	{ "-nodups",	'1',	O(nodups) },
	{ "\0\1",		's',	ls_input },
	{ "",			0,		NULL }
};
#undef O

static void list_sort_free(struct list_sort *ls)
{
	ffvec_free(&ls->input);
	ffmem_free(ls);
}

struct ffarg_ctx list_sort_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct list_sort), list_sort_free, ls_action, list_sort_args);
}
