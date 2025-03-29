/** phiola: executor: 'server' command
2025, Simon Zolin */

static int server_help()
{
	help_info_write("\
Start audio streaming server:\n\
	`phiola server` INPUT... [OPTIONS]\n\
\n\
INPUT                   File name or a directory\n\
                          `@names`  Read file names from standard input\n\
\n\
Options:\n\
  `-include` WILDCARD     Only include files matching a wildcard (case-insensitive)\n\
  `-exclude` WILDCARD     Exclude files & directories matching a wildcard (case-insensitive)\n\
\n\
  `-opus_quality` NUMBER  Opus encoding bitrate:\n\
                          6..510 (VBR)\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_srv {
	ffvec	include, exclude; // ffstr[]
	ffvec	input; // ffstr[]
	const char*	aac_profile;
	uint	opus_q;
};

static int srv_include(struct cmd_srv *s, ffstr ss)
{
	*ffvec_pushT(&s->include, ffstr) = ss;
	return 0;
}

static int srv_exclude(struct cmd_srv *s, ffstr ss)
{
	*ffvec_pushT(&s->exclude, ffstr) = ss;
	return 0;
}

static int srv_input(struct cmd_srv *s, ffstr fn)
{
	if (fn.len && fn.ptr[0] == '-')
		return _ffargs_err(&x->cmd, 1, "unknown option '%S'. Use '-h' for usage info.", &fn);

	return cmd_input(&s->input, fn);
}

static int srv_prepare(struct cmd_srv *s)
{
	if (!s->input.len)
		return _ffargs_err(&x->cmd, 1, "please specify input file");
   return 0;
}

static void playlist_prepare(struct cmd_srv *s)
{
	struct phi_queue_conf qc = {
		.tconf = {
			.ifile = {
				.include = *(ffslice*)&s->include,
				.exclude = *(ffslice*)&s->exclude,
			},
		},
	};
	x->queue->create(&qc);
	ffstr *it;
	FFSLICE_WALK(&s->input, it) {
		struct phi_queue_entry qe = {
			.url = it->ptr,
		};
		x->queue->add(NULL, &qe);
	}
	ffvec_free(&s->input);
}

static int srv_action(struct cmd_srv *s)
{
	playlist_prepare(s);

	struct phi_track_conf c = {
		.ofile.name = ffsz_dup("stream.opus"),
	};
	c.opus.bitrate = s->opus_q;

	const phi_track_if *track = x->core->track;
	phi_track *t = track->create(&c);
	track->filter(t, x->core->mod("http.server"), 0);
	track->start(t);
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_srv, m)
static const struct ffarg cmd_srv[] = {
	{ "-exclude",		'+S',	srv_exclude },
	{ "-help",			0,		server_help },
	{ "-include",		'+S',	srv_include },
	{ "-opus_quality",	'u',	O(opus_q) },
	{ "\0\1",			'S',	srv_input },
	{ "",				0,		srv_prepare },
};
#undef O

static void cmd_srv_free(struct cmd_srv *s)
{
	ffmem_free(s);
}

static struct ffarg_ctx cmd_server_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct cmd_srv), cmd_srv_free, srv_action, cmd_srv);
}
