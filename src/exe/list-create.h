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

	phi_queue_id q;
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

static void lc_saved(void *param, phi_track *t)
{
	x->exit_code = t->error & 0xff;
	x->core->sig(PHI_CORE_STOP);
}

static void lc_ready(struct list_create *lc)
{
	x->subcmd.q_expanded = NULL;

	x->queue->remove_multi(lc->q, PHI_Q_RM_DUPS);

	if (x->queue->save(lc->q, lc->output, lc_saved, lc))
		goto err;
	return;

err:
	x->exit_code = 1;
	x->core->sig(PHI_CORE_STOP);
}

static int lc_action(struct list_create *lc)
{
	x->queue->on_change(q_on_change);
	struct phi_queue_conf qc = {
		.tconf = {
			.ifile.include = *(ffslice*)&lc->include,
			.ifile.exclude = *(ffslice*)&lc->exclude,
		},
	};
	lc->q = x->queue->create(&qc);

	char **it;
	FFSLICE_WALK(&lc->input, it) {
		struct phi_queue_entry qe = {
			.url = *it,
		};
		x->queue->add(lc->q, &qe);
	}

	if (!x->queue->status(lc->q))
		lc_ready(lc);
	else
		x->subcmd.q_expanded = (phi_task_func)lc_ready;
	return 0;
}

#define O(m)  (void*)FF_OFF(struct list_create, m)
static const struct ffarg list_create_args[] = {
	{ "-exclude",	'+S',	lc_exclude },
	{ "-help",		'1',	lc_help },
	{ "-include",	'+S',	lc_include },
	{ "-out",		's',	O(output) },
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
