/** phiola: executor: 'list create' command
2023, Simon Zolin */

static int lc_help()
{
	help_info_write("\
Create playlist file\n\
\n\
    `phiola list create` [INPUT...] -o file.m3u\n\
\n\
  INPUT             File name, directory or URL\n\
\n\
Options:\n\
\n\
  `-include` WILDCARD     Only include files matching a wildcard (case-insensitive)\n\
  `-exclude` WILDCARD     Exclude files & directories matching a wildcard (case-insensitive)\n\
  `-out` FILE             Output file name\n\
");
	x->exit_code = 0;
	return 1;
}

struct list_create {
	ffvec	include, exclude; // ffstr[]
	ffvec	input; // char*[]
	const char *output;
	u_char	unique;

	phi_task task;
};

static int lc_input(struct list_create *lc, const char *fn)
{
	*ffvec_pushT(&lc->input, const char*) = fn;
	return 0;
}

static int lc_include(struct list_create *lc, ffstr s)
{
	*ffvec_pushT(&lc->include, ffstr) = s;
	return 0;
}

static int lc_exclude(struct list_create *lc, ffstr s)
{
	*ffvec_pushT(&lc->exclude, ffstr) = s;
	return 0;
}

static void lc_done(void *udata, int result)
{
	x->exit_code = result;
	x->core->sig(PHI_CORE_STOP);
}

static int lc_action(struct list_create *lc)
{
	const phi_playlist_if *pl_if = x->core->mod("format.playlist");
	if (!pl_if)
		return 1;
	struct phi_playlist_conf c = {
		.include_str = *(ffslice*)&lc->include,
		.exclude_str = *(ffslice*)&lc->exclude,
		.on_complete = lc_done,
		.udata = lc,
	};
	pl_if->create(lc->output, *(ffslice*)&lc->input, 0, &c);
	return 0;
}

#define O(m)  (void*)FF_OFF(struct list_create, m)
static const struct ffarg list_create_args[] = {
	{ "-exclude",	'+S',	lc_exclude },
	{ "-help",		'1',	lc_help },
	{ "-include",	'+S',	lc_include },
	{ "-out",		's',	O(output) },
	{ "-unique",	'1',	O(unique) }, // obsolete
	{ "\0\1",		's',	lc_input },
	{ "",			0,		NULL }
};
#undef O

static void list_create_free(struct list_create *lc)
{
	ffvec_free(&lc->input);
	ffvec_free(&lc->include);
	ffvec_free(&lc->exclude);
	ffmem_free(lc);
}

static struct ffarg_ctx list_create_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct list_create), list_create_free, lc_action, list_create_args);
}
