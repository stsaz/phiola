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

	ffvec	tasks; // fftask[]
	uint	counter;
	int		result;
};

static int lh_input(struct list_heal *lh, const char *fn)
{
	*ffvec_pushT(&lh->input, const char*) = fn;
	return 0;
}

static void lh_done(void *param, int result)
{
	struct list_heal *lh = param;
	lh->result |= result;
	if (ffint_fetch_add(&lh->counter, -1) - 1)
		return;

	x->exit_code = lh->result;
	x->core->sig(PHI_CORE_STOP);
}

static int lh_action(struct list_heal *lh)
{
	lh->counter = lh->input.len;

	const phi_playlist_if *pl_if = x->core->mod("format.playlist");
	if (!pl_if)
		return 1;
	struct phi_playlist_conf c = {
		.on_complete = lh_done,
		.udata = lh,
	};

	char **it;
	FFSLICE_WALK(&lh->input, it) {
		pl_if->heal(*it, 0, &c);
	}
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
