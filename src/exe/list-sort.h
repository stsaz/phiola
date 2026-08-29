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

	uint	counter;
	int		result;
};

static int ls_input(struct list_sort *ls, const char *fn)
{
	*ffvec_pushT(&ls->input, const char*) = fn;
	return 0;
}

static void ls_done(void *param, int result)
{
	struct list_sort *ls = param;
	ls->result |= result;
	if (ffint_fetch_add(&ls->counter, -1) - 1)
		return;

	x->exit_code = ls->result;
	x->core->sig(PHI_CORE_STOP);
}

static int ls_action(struct list_sort *ls)
{
	ls->counter = ls->input.len;

	const phi_playlist_if *pl_if = x->core->mod("format.playlist");
	if (!pl_if)
		return 1;
	struct phi_playlist_conf c = {
		.on_complete = ls_done,
		.udata = ls,
	};

	char **it;
	FFSLICE_WALK(&ls->input, it) {
		pl_if->sort(*it, 0, &c);
	}
	return 0;
}

#define O(m)  (void*)FF_OFF(struct list_sort, m)
static const struct ffarg list_sort_args[] = {
	{ "-help",		'1',	ls_help },
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
