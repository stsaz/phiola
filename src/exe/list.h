/** phiola: executor: 'list' command
2023, Simon Zolin */

static int lc_help()
{
	static const char s[] = "\
Create playlist file\n\
\n\
    phiola list create [INPUT...] -o file.m3u\n\
\n\
  INPUT             File name, directory or URL\n\
\n\
Options:\n\
\n\
  -include WILDCARD     Only include files matching a wildcard (case-insensitive)\n\
  -exclude WILDCARD     Exclude files & directories matching a wildcard (case-insensitive)\n\
  -out FILE             Output file name\n\
";
	ffstdout_write(s, FFS_LEN(s));
	x->exit_code = 0;
	return 1;
}

struct list_create {
	ffvec	include, exclude; // ffstr[]
	ffvec	input; // char*[]
	const char *output;
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

static void lc_done(void *lc, phi_track *t)
{
	x->exit_code = t->error & 0xff;
	x->core->sig(PHI_CORE_STOP);
}

static int lc_action(struct list_create *lc)
{
	struct phi_queue_conf qc = {};
	phi_queue_id q = x->queue->create(&qc);

	char **it;
	FFSLICE_WALK(&lc->input, it) {
		struct phi_queue_entry qe = {
			.conf.ifile = {
				.name = ffsz_dup(*it),
				.include = *(ffslice*)&lc->include,
				.exclude = *(ffslice*)&lc->exclude,
			},
		};
		x->queue->add(q, &qe);
	}
	ffvec_free(&lc->input);

	// it's safe to call this immediately because 'add' task will complete before 'save'
	if (x->queue->save(q, lc->output, lc_done, NULL))
		return 1;
	return 0;
}

static int lc_fin(struct list_create *lc)
{
	return 0;
}

#define O(m)  (void*)FF_OFF(struct list_create, m)
static const struct ffarg list_create_args[] = {
	{ "-exclude",	'S',	lc_exclude },
	{ "-help",		'1',	lc_help },
	{ "-include",	'S',	lc_include },
	{ "-out",		's',	O(output) },
	{ "\0\1",		's',	lc_input },
	{ "",			'1',	lc_fin }
};
#undef O

static void list_create_free(struct list_create *lc)
{
	ffvec_free(&lc->include);
	ffvec_free(&lc->exclude);
	ffmem_free(lc);
}

static struct ffarg_ctx list_create_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct list_create), list_create_free, lc_action, list_create_args);
}

#include <exe/list-sort.h>

static int list_help()
{
	static const char s[] = "\
Process playlist files\n\
\n\
    phiola list COMMAND [OPTIONS]\n\
\n\
COMMAND:\n\
\n\
  create            Create playlist file\n\
  sort              Sort playlist\n\
\n\
Use 'phiola list COMMAND -h' for more info.\n\
";
	ffstdout_write(s, FFS_LEN(s));
	x->exit_code = 0;
	return 1;
}

const struct ffarg cmd_list_args[] = {
	{ "-help",		'1',	list_help },
	{ "create",		'{',	list_create_init },
	{ "sort",		'{',	list_sort_init },
	{}
};
