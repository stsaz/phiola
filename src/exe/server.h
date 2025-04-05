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
  `-shuffle`              Randomize input queue\n\
\n\
  `-aac_quality` NUMBER   AAC encoding bitrate:\n\
                          8..800 (CBR, kbit/s)\n\
  `-opus_quality` NUMBER  Opus encoding bitrate (VBR):\n\
                          6..510 (default: 128)\n\
  `-channels` NUMBER      Channels number (default: 2)\n\
\n\
  `-port` NUMBER          TCP port (default: 21014)\n\
  `-max_clients` NUMBER   Max. number of clients\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_srv {
	ffvec	include, exclude; // ffstr[]
	ffvec	input; // ffstr[]
	uint	aac_q;
	uint	channels;
	uint	opus_q;
	uint	port;
	u_char	shuffle;

	struct phi_asv_conf ac;
	phi_task task;
};

static void srv_action2(struct cmd_srv *s);

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

	x->max_tasks = s->ac.max_clients;
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
	phi_queue_id q = x->queue->create(&qc);
	ffstr *it;
	FFSLICE_WALK(&s->input, it) {
		struct phi_queue_entry qe = {
			.url = it->ptr,
		};
		x->queue->add(q, &qe);
	}
	ffvec_free(&s->input);
}

static int srv_action(struct cmd_srv *s)
{
	playlist_prepare(s);
	x->core->task(0, &s->task, (void*)srv_action2, s);
	return 0;
}

static void srv_action2(struct cmd_srv *s)
{
	if (s->shuffle)
		x->queue->sort(NULL, PHI_Q_SORT_RANDOM);

	s->ac.port = (s->port > 0 && s->port < 0xffff) ? s->port : 21014;

	struct phi_track_conf c = {
		.oaudio = {
			.buf_time = 1000,
		},
	};
	*(struct phi_asv_conf**)&c.ofile.mtime = &s->ac;

	if (s->aac_q) {
		struct phi_af f = {
			.format = PHI_PCM_16,
			.rate = 48000,
			.channels = (s->channels) ? s->channels : 2,
			.interleaved = 1,
		};
		c.oaudio.format = f;
		c.aac.quality = (s->aac_q >= 8) ? s->aac_q : 128;
		c.ofile.name = ffsz_dup("stream.aac");

	} else {
		struct phi_af f = {
			.format = PHI_PCM_FLOAT32,
			.rate = 48000,
			.channels = (s->channels) ? s->channels : 2,
			.interleaved = 1,
		};
		c.oaudio.format = f;
		c.opus.bitrate = (s->opus_q >= 6) ? s->opus_q : 128;
		c.ofile.name = ffsz_dup("stream.opus");
	}

	const phi_track_if *track = x->core->track;
	phi_track *t = track->create(&c);
	track->filter(t, x->core->mod("http.server"), 0);
	track->start(t);
}

#define O(m)  (void*)FF_OFF(struct cmd_srv, m)
static const struct ffarg cmd_srv[] = {
	{ "-aac_quality",	'u',	O(aac_q) },
	{ "-channels",		'u',	O(channels) },
	{ "-exclude",		'+S',	srv_exclude },
	{ "-help",			0,		server_help },
	{ "-include",		'+S',	srv_include },
	{ "-max_clients",	'u',	O(ac.max_clients) },
	{ "-opus_quality",	'u',	O(opus_q) },
	{ "-port",			'u',	O(port) },
	{ "-shuffle",		'1',	O(shuffle) },
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
