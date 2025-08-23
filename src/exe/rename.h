/** phiola: executor: 'rename' command
2025, Simon Zolin */

static int rename_help()
{
	help_info_write("\
Auto-rename files:\n\
    `phiola rename` INPUT... -o PATTERN\n\
\n\
INPUT                   File name, directory or URL\n\
                          `@names`  Read file names from standard input\n\
\n\
Options:\n\
  `-o` PATTERN            Target file name pattern (without file extension).\n\
                        Supports runtime variable expansion:\n\
                          @STRING    Expands to file meta data,\n\
                                       e.g. `-o \"@tracknumber. @artist - @title\"`\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_rename {
	phi_task task;
	ffvec input; // ffstr[]
	const char *output;
};

static int rename_action(struct cmd_rename *r)
{
	x->queue->on_change(q_on_change);

	struct phi_queue_conf qc = {
		.first_filter = &phi_guard,
	};
	x->queue->create(&qc);

	ffstr *s;
	FFSLICE_WALK(&r->input, s) {
		struct phi_queue_entry qe = {
			.url = s->ptr,
		};
		x->queue->add(NULL, &qe);
	}
	ffvec_free(&r->input);

	x->queue->rename_all(NULL, r->output, 0);
	return 0;
}

static int rename_input(struct cmd_rename *r, ffstr s)
{
	return cmd_input(&r->input, s);
}

static int rename_fin(struct cmd_rename *r)
{
	if (!r->input.len)
		return _ffargs_err(&x->cmd, 1, "please specify input file");

	if (!r->output)
		return _ffargs_err(&x->cmd, 1, "please specify output file name pattern with '-o PATTERN'");

	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_rename, m)
static const struct ffarg cmd_rename[] = {
	{ "-help",		0,		rename_help },
	{ "-o",			's',	O(output) },
	{ "\0\1",		'S',	rename_input },
	{ "",			0,		rename_fin },
};
#undef O

static void cmd_rename_free(struct cmd_rename *r)
{
	ffmem_free(r);
}

struct ffarg_ctx cmd_rename_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct cmd_rename), cmd_rename_free, rename_action, cmd_rename);
}
