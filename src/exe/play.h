/** phiola: executor: 'play' command
2023, Simon Zolin */

static int play_help()
{
	help_info_write("\
Play audio:\n\
    `phiola play` [OPTIONS] INPUT...\n\
\n\
INPUT                   File name, directory or URL\n\
                          `@stdin`  Read from standard input\n\
                          `@names`  Read file names from standard input\n\
\n\
Options:\n\
  `-include` WILDCARD     Only include files matching a wildcard (case-insensitive)\n\
  `-exclude` WILDCARD     Exclude files & directories matching a wildcard (case-insensitive)\n\
  `-rbuffer` SIZE         Read-buffer size (in KB units)\n\
  `-tee` FILE             Copy input data to a file.\n\
                          `@stdout`    Write to standard output\n\
                        File extension should match input data; audio conversion is never performed.\n\
                        Supports runtime variable expansion:\n\
                          `@nowdate`   Current date\n\
                          `@nowtime`   Current time\n\
                          `@counter`   Sequentially incremented number\n\
                          `@STRING`    Expands to file meta data,\n\
                                       e.g. `-tee \"@artist - @title.mp3\"`\n\
  `-dup` FILE.wav         Copy output data to a file.\n\
                          `@stdout`    Write to standard output\n\
                        Supports runtime variable expansion (see `-tee`)\n\
\n\
  `-repeat_all`           Repeat all tracks\n\
  `-random`               Choose the next track randomly\n\
  `-number` NUMBER        Exit after N tracks played\n\
  `-tracks` NUMBER[,...]  Select only specific tracks in a .cue list\n\
\n\
  `-seek` TIME            Seek to time:\n\
                          [[HH:]MM:]SS[.MSC]\n\
  `-until` TIME           Stop at time\n\
\n\
  `-rgnorm`               ReplayGain normalizer\n\
  `-norm` \"OPTIONS\"       Auto loudness normalizer. Options:\n\
                          `target`     Integer\n\
                          `attenuate`  Integer\n\
                          `gain`       Integer\n\
\n\
  `-audio` STRING         Audio library name (e.g. alsa)\n\
  `-device` NUMBER        Playback device number\n\
  `-exclusive`            Open device in exclusive mode (WASAPI)\n\
  `-buffer` NUMBER        Length (in msec) of the playback buffer\n\
\n\
  `-perf`                 Print performance counters\n\
  `-remote`               Listen for incoming remote commands\n\
  `-remote_id` STRING     phiola instance ID\n\
  `-volume` NUMBER        Set initial volume level: 0..100\n\
  `-connect_timeout` NUMBER\n\
                          Connection timeout (in seconds): 1..255\n\
  `-recv_timeout` NUMBER  Receive timeout (in seconds): 1..255\n\
  `-no_meta`              Disable ICY meta data\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_play {
	char*	audio_module;
	const char*	auto_norm;
	const char*	dup;
	const char*	tee;
	const char*	remote_id;
	ffstr	audio;
	ffvec	include, exclude; // ffstr[]
	ffvec	input; // ffstr[]
	ffvec	tracks; // uint[]
	u_char	exclusive;
	u_char	no_meta;
	u_char	perf;
	u_char	random;
	u_char	remote;
	u_char	repeat_all;
	u_char	rg_norm;
	uint	buffer;
	uint	connect_timeout;
	uint	device;
	uint	number;
	uint	rbuffer_kb;
	uint	recv_timeout;
	uint	volume;
	uint64	seek;
	uint64	until;
};

static int play_seek(struct cmd_play *p, ffstr s) { return cmd_time_value(&p->seek, s); }

static int play_until(struct cmd_play *p, ffstr s) { return cmd_time_value(&p->until, s); }

static int play_include(struct cmd_play *p, ffstr s)
{
	*ffvec_pushT(&p->include, ffstr) = s;
	return 0;
}

static int play_exclude(struct cmd_play *p, ffstr s)
{
	*ffvec_pushT(&p->exclude, ffstr) = s;
	return 0;
}

static int play_tracks(struct cmd_play *p, ffstr s) { return cmd_tracks(&p->tracks, s); }

static int play_input(struct cmd_play *p, ffstr s)
{
	if (s.len && s.ptr[0] == '-')
		return _ffargs_err(&x->cmd, 1, "unknown option '%S'. Use '-h' for usage info.", &s);

	x->stdin_busy = ffstr_eqz(&s, "@stdin");
	return cmd_input(&p->input, s);
}

static int play_grd_process(void *f, phi_track *t)
{
	struct cmd_play *p = x->subcmd.obj;
	if (p->number) {
		if (0 == --p->number)
			t->chain_flags |= PHI_FSTOP_AFTER;
	}
	return phi_grd_process(f, t);
}

static const phi_filter play_guard = {
	NULL, phi_grd_close, play_grd_process,
	"play-guard"
};

static int play_action(struct cmd_play *p)
{
	if (p->volume != ~0U) {
		struct phi_ui_conf uc = {
			.volume_percent = p->volume,
		};
		const phi_ui_if *uif = x->core->mod("tui.if");
		uif->conf(&uc);
	}

	if (p->audio.len)
		p->audio_module = ffsz_allocfmt("%S.play", &p->audio);

	struct phi_track_conf c = {
		.ifile = {
			.buf_size = p->rbuffer_kb * 1024,
			.include = *(ffslice*)&p->include,
			.exclude = *(ffslice*)&p->exclude,
			.connect_timeout_sec = ffmin(p->connect_timeout, 0xff),
			.recv_timeout_sec = ffmin(p->recv_timeout, 0xff),
			.no_meta = p->no_meta,
		},
		.tee = (p->tee) ? p->tee : p->dup,
		.tee_output = !!p->dup,
		.tracks = *(ffslice*)&p->tracks,
		.seek_msec = p->seek,
		.until_msec = p->until,
		.afilter = {
			.rg_normalizer = (p->rg_norm && !p->auto_norm),
			.auto_normalizer = p->auto_norm,
		},
		.oaudio = {
			.device_index = p->device,
			.buf_time = p->buffer,
			.exclusive = p->exclusive,
		},
		.print_time = p->perf,
	};

	x->queue->on_change(q_on_change);
	struct phi_queue_conf qc = {
		.first_filter = &play_guard,
		.audio_module = p->audio_module,
		.ui_module = "tui.play",
		.tconf = c,
		.random = p->random,
		.repeat_all = p->repeat_all,
	};
	x->queue->create(&qc);
	ffstr *it;
	FFSLICE_WALK(&p->input, it) {
		struct phi_queue_entry qe = {
			.url = it->ptr,
		};
		x->queue->add(NULL, &qe);
	}
	ffvec_free(&p->input);

	x->queue->play(NULL, NULL);

	if (p->remote || p->remote_id) {
		const phi_remote_sv_if *rsv = x->core->mod("remote.server");
		if (rsv->start(p->remote_id))
			return -1;
	}
	return 0;
}

static int play_check(struct cmd_play *p)
{
	if (!p->input.len)
		return _ffargs_err(&x->cmd, 1, "please specify input file");

	if (p->tee && p->dup)
		return _ffargs_err(&x->cmd, 1, "-tee and -dup can not be used together");

	if (p->tee || p->dup) {
		ffstr name;
		ffpath_splitname_str(FFSTR_Z((p->tee) ? p->tee : p->dup), &name, NULL);
		x->stdout_busy = ffstr_eqz(&name, "@stdout");
	}

	if (p->buffer)
		x->timer_int_msec = ffmin(p->buffer / 2, x->timer_int_msec);
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_play, m)
static const struct ffarg cmd_play[] = {
	{ "-audio",			'S',	O(audio) },
	{ "-buffer",		'u',	O(buffer) },
	{ "-connect_timeout",'u',	O(connect_timeout) },
	{ "-device",		'u',	O(device) },
	{ "-dup",			's',	O(dup) },
	{ "-exclude",		'+S',	play_exclude },
	{ "-exclusive",		'1',	O(exclusive) },
	{ "-help",			0,		play_help },
	{ "-include",		'+S',	play_include },
	{ "-no_meta",		'1',	O(no_meta) },
	{ "-norm",			's',	O(auto_norm) },
	{ "-number",		'u',	O(number) },
	{ "-perf",			'1',	O(perf) },
	{ "-random",		'1',	O(random) },
	{ "-rbuffer",		'u',	O(rbuffer_kb) },
	{ "-recv_timeout",	'u',	O(recv_timeout) },
	{ "-remote",		'1',	O(remote) },
	{ "-remote_id",		's',	O(remote_id) },
	{ "-repeat_all",	'1',	O(repeat_all) },
	{ "-rgnorm",		'1',	O(rg_norm) },
	{ "-seek",			'S',	play_seek },
	{ "-tee",			's',	O(tee) },
	{ "-tracks",		'S',	play_tracks },
	{ "-until",			'S',	play_until },
	{ "-volume",		'u',	O(volume) },
	{ "\0\1",			'S',	play_input },
	{ "",				0,		play_check },
};
#undef O

static void cmd_play_free(struct cmd_play *p)
{
	ffvec_free(&p->include);
	ffvec_free(&p->exclude);
	ffmem_free(p->audio_module);
	ffmem_free(p);
}

static struct ffarg_ctx cmd_play_init(void *obj)
{
	struct cmd_play *p = ffmem_new(struct cmd_play);
	p->volume = ~0U;
	return SUBCMD_INIT(p, cmd_play_free, play_action, cmd_play);
}
