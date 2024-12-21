/** phiola: executor: 'tag' command
2023, Simon Zolin */

static int tag_help()
{
	help_info_write("\
Edit file tags:\n\
    `phiola tag` OPTIONS FILE...\n\
\n\
Options:\n\
  `-clear`                Remove all existing tags.  By default all original tags are preserved.\n\
  `-meta` NAME=VALUE      Meta data\n\
                          .mp3 supports: album, albumartist, artist, comment, date, genre, picture, publisher, title, tracknumber, tracktotal.\n\
  `-preserve_date`        Preserve file modification date\n\
  `-fast`                 Fail if need to rewrite whole file\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_tag {
	ffvec	input;
	ffvec	meta;
	u_char	clear;
	u_char	preserve_date;
	u_char	fast;
};

static int tag_input(struct cmd_tag *t, ffstr s)
{
	if (s.len && s.ptr[0] == '-')
		return _ffargs_err(&x->cmd, 1, "unknown option '%S'. Use '-h' for usage info.", &s);

	return cmd_input(&t->input, s);
}

static int tag_meta(struct cmd_tag *t, ffstr s)
{
	ffstr name, val;
	if (ffstr_splitby(&s, '=', &name, &val) <= 0)
		return _ffargs_err(&x->cmd, 1, "invalid meta: '%S'", &s);
	*ffvec_pushT(&t->meta, ffstr) = s;
	return 0;
}

static int tag_action(struct cmd_tag *t)
{
	const phi_tag_if *tag = x->core->mod("format.tag");
	int r = 0;

	ffstr *fn;
	FFSLICE_WALK(&t->input, fn) {
		struct phi_tag_conf conf = {
			.filename = ffsz_dupstr(fn),
			.meta = t->meta,
			.clear = t->clear,
			.preserve_date = t->preserve_date,
			.no_expand = t->fast,
		};
		r |= tag->edit(&conf);
	}

	x->core->sig(PHI_CORE_STOP);
	x->exit_code = (!r) ? 0 : 1;
	return 0;
}

static int tag_prepare(struct cmd_tag *t)
{
	if (!t->input.len)
		return _ffargs_err(&x->cmd, 1, "please specify input file");
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_tag, m)
static const struct ffarg cmd_tag_args[] = {
	{ "-clear",			'1',	O(clear) },
	{ "-fast",			'1',	O(fast) },
	{ "-help",			0,		tag_help },
	{ "-meta",			'+S',	tag_meta },
	{ "-preserve_date",	'1',	O(preserve_date) },
	{ "\0\1",			'S',	tag_input },
	{ "",				0,		tag_prepare },
};
#undef O

static void cmd_tag_free(struct cmd_tag *t)
{
	ffvec_free(&t->input);
	ffvec_free(&t->meta);
	ffmem_free(t);
}

struct ffarg_ctx cmd_tag_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct cmd_tag), cmd_tag_free, tag_action, cmd_tag_args);
}
