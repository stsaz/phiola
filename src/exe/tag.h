/** phiola: executor: 'tag' command
2023, Simon Zolin */

static int tag_help()
{
	help_info_write("\
Edit file tags:\n\
    `phiola tag` OPTIONS FILE...\n\
\n\
Options:\n\
  `-include` WILDCARD     `-rg`: Only include files matching a wildcard (case-insensitive)\n\
  `-exclude` WILDCARD     `-rg`: Exclude files & directories matching a wildcard (case-insensitive)\n\
\n\
  `-clear`                Remove all existing tags.  By default all original tags are preserved.\n\
  `-meta` NAME=VALUE      Meta data\n\
  `-rg` \"OPTIONS\"         Write ReplayGain tags. Options:\n\
                          `track_gain` (default)\n\
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
	uint	replay_gain;
	ffvec	include, exclude; // ffstr[]

	const phi_tag_if *tag;
};

static int tag_input(struct cmd_tag *t, ffstr s)
{
	if (s.len && s.ptr[0] == '-')
		return _ffargs_err(&x->cmd, 1, "unknown option '%S'. Use '-h' for usage info.", &s);

	return cmd_input(&t->input, s);
}

static int tag_include(struct cmd_tag *t, ffstr s)
{
	*ffvec_pushT(&t->include, ffstr) = s;
	return 0;
}

static int tag_exclude(struct cmd_tag *t, ffstr s)
{
	*ffvec_pushT(&t->exclude, ffstr) = s;
	return 0;
}

static int tag_meta(struct cmd_tag *t, ffstr s)
{
	ffstr name, val;
	if (ffstr_splitby(&s, '=', &name, &val) <= 0)
		return _ffargs_err(&x->cmd, 1, "invalid meta: '%S'", &s);
	*ffvec_pushT(&t->meta, ffstr) = s;
	return 0;
}

static int tag_replay_gain(struct cmd_tag *t, const char *s)
{
	struct rg {
		u_char track_gain;
	} rg;

	#define O(m)  (void*)(size_t)FF_OFF(struct rg, m)
	static const struct ffarg rg_args[] = {
		{ "track_gain",		'1',	O(track_gain) },
		{}
	};
	#undef O

	struct ffargs a = {};
	if (ffargs_process_line(&a, rg_args, &rg, FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES, s))
		return _ffargs_err(&x->cmd, 1, "%s", a.error);

	if (rg.track_gain || !*s) {
		t->replay_gain |= 1;
	}
	return 0;
}

static void tag_grd_close(void *f, phi_track *t)
{
	struct cmd_tag *tt = x->subcmd.obj;
	ffvec tags = {};
	ffstr s = {};
	int rc = -1;

	x->core->track->stop(t);

	if (t->error)
		goto end;

	// -18: ReplayGain target
	// -1: EBU R 128: "The Maximum True-Peak Level in production shall not exceed −1 dBTP"
	// e.g. -10 loudness -> -19 target = -9 gain
	double rg = -18-1 - t->oaudio.loudness;

	char val[8]; // "-xx.xx"
	uint n = ffs_fromfloat(rg, val, sizeof(val) - 1, FFS_FLTKEEPSIGN | FFS_FLTWIDTH(3) | FFS_FLTZERO | 2);
	if (!n) {
		phi_errlog(x->core, NULL, t, "ffs_fromfloat");
		goto end;
	}
	val[n] = '\0';

	ffvec_add2T(&tags, &tt->meta, ffstr);
	size_t cap = 0;
	ffstr_growfmt(&s, &cap, "replaygain_track_gain=%s", val);
	*ffvec_pushT(&tags, ffstr) = s;

	struct phi_tag_conf conf = {
		.filename = t->conf.ifile.name,
		.meta = *(ffslice*)&tags,
		.clear = tt->clear,
		.preserve_date = tt->preserve_date,
		.no_expand = tt->fast,
	};
	if (tt->tag->edit(&conf))
		goto end;

	rc = 0;

end:
	ffstr_free(&s);
	ffvec_free(&tags);
	if (rc)
		x->exit_code = 1;
}

static const phi_filter tag_guard = {
	NULL, tag_grd_close, phi_grd_process,
	"tag-guard"
};

static int tag_action(struct cmd_tag *t)
{
	t->tag = x->core->mod("format.tag");
	int r = 0;

	if (t->replay_gain) {
		x->queue->on_change(q_on_change);

		struct phi_track_conf c = {
			.ifile = {
				.include = *(ffslice*)&t->include,
				.exclude = *(ffslice*)&t->exclude,
			},
			.afilter.loudness_summary = 1,
			.cross_worker_assign = 1,
		};

		struct phi_queue_conf qc = {
			.first_filter = &tag_guard,
			.ui_module = "tui.play",
			.tconf = c,
			.analyze = 1,
		};
		x->queue->create(&qc);

		ffstr *it;
		FFSLICE_WALK(&t->input, it) {
			struct phi_queue_entry qe = {
				.url = it->ptr,
			};
			x->queue->add(NULL, &qe);
		}
		ffvec_free(&t->input);

		x->queue->play(NULL, NULL);
		return 0;
	}

	ffstr *fn;
	FFSLICE_WALK(&t->input, fn) {
		struct phi_tag_conf conf = {
			.filename = fn->ptr,
			.meta = *(ffslice*)&t->meta,
			.clear = t->clear,
			.preserve_date = t->preserve_date,
			.no_expand = t->fast,
		};
		r |= t->tag->edit(&conf);
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
	{ "-exclude",		'+S',	tag_exclude },
	{ "-fast",			'1',	O(fast) },
	{ "-help",			0,		tag_help },
	{ "-include",		'+S',	tag_include },
	{ "-meta",			'+S',	tag_meta },
	{ "-preserve_date",	'1',	O(preserve_date) },
	{ "-rg",			's',	tag_replay_gain },
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
