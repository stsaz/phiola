/** phiola: executor: 'play' command
2023, Simon Zolin */

static int play_help()
{
	static const char s[] = "\
Play audio:\n\
    phiola play [OPTIONS] INPUT...\n\
\n\
INPUT                   File name, directory or URL\n\
                          @stdin  Read from standard input\n\
\n\
Options:\n\
  -include WILDCARD     Only include files matching a wildcard (case-insensitive)\n\
  -exclude WILDCARD     Exclude files & directories matching a wildcard (case-insensitive)\n\
\n\
  -repeat-all           Repeat all tracks\n\
  -random               Choose the next track randomly\n\
  -tracks NUMBER[,...]  Select only specific tracks in a .cue list\n\
\n\
  -seek TIME            Seek to time:\n\
                          [[HH:]MM:]SS[.MSC]\n\
  -until TIME           Stop at time\n\
\n\
  -danorm \"OPTIONS\"     Apply Dynamic Audio Normalizer filter. Options:\n\
                          frame       Integer\n\
                          size        Integer\n\
                          peak        Float\n\
                          max-amp     Float\n\
                          target-rms  Float\n\
                          compress    Float\n\
\n\
  -audio STRING         Audio library name (e.g. alsa)\n\
  -device NUMBER        Playback device number\n\
  -exclusive            Open device in exclusive mode (WASAPI)\n\
  -buffer NUMBER        Length (in msec) of the playback buffer\n\
\n\
  -perf                 Print performance counters\n\
";
	ffstdout_write(s, FFS_LEN(s));
	x->exit_code = 0;
	return 1;
}

struct cmd_play {
	ffvec input; // ffstr[]
	ffvec include, exclude; // ffstr[]
	ffstr audio;
	char *audio_module;
	uint buffer;
	uint device;
	uint64 seek;
	uint64 until;
	const char *danorm;
	ffbyte random;
	ffbyte repeat_all;
	ffbyte perf;
	ffbyte exclusive;
	ffvec tracks; // uint[]
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

static void play_qu_add(struct cmd_play *p, ffstr *fn)
{
	struct phi_track_conf c = {
		.ifile = {
			.name = ffsz_dupstr(fn),
			.include = *(ffslice*)&p->include,
			.exclude = *(ffslice*)&p->exclude,
		},
		.tracks = *(ffslice*)&p->tracks,
		.seek_msec = p->seek,
		.until_msec = p->until,
		.afilter = {
			.danorm = p->danorm,
		},
		.oaudio = {
			.device_index = p->device,
			.buf_time = p->buffer,
			.exclusive = p->exclusive,
		},
		.print_time = p->perf,
	};

	struct phi_queue_entry qe = {
		.conf = c,
	};
	x->queue->add(NULL, &qe);
}

static int play_action()
{
	struct cmd_play *p = x->cmd_data;

	if (p->audio.len)
		p->audio_module = ffsz_allocfmt("%S.play", &p->audio);

	struct phi_queue_conf qc = {
		.name = "play",
		.first_filter = &phi_guard,
		.audio_module = p->audio_module,
		.ui_module = "tui.play",
		.random = p->random,
		.repeat_all = p->repeat_all,
	};
	x->queue->create(&qc);
	ffstr *it;
	FFSLICE_WALK(&p->input, it) {
		play_qu_add(p, it);
	}
	ffvec_free(&p->input);

	x->queue->play(NULL, NULL);
	return 0;
}

static int play_check(struct cmd_play *p)
{
	if (!p->input.len)
		return _ffargs_err(&x->cmd, 1, "please specify input file");

	x->action = play_action;
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_play, m)
static const struct ffarg cmd_play[] = {
	{ "-audio",		'S',	O(audio) },
	{ "-buffer",	'u',	O(buffer) },
	{ "-danorm",	's',	O(danorm) },
	{ "-device",	'u',	O(device) },
	{ "-exclude",	'S',	play_exclude },
	{ "-exclusive",	'1',	O(exclusive) },
	{ "-help",		0,		play_help },
	{ "-include",	'S',	play_include },
	{ "-perf",		'1',	O(perf) },
	{ "-random",	'1',	O(random) },
	{ "-repeat-all",'1',	O(repeat_all) },
	{ "-seek",		'S',	play_seek },
	{ "-tracks",	'S',	play_tracks },
	{ "-until",		'S',	play_until },
	{ "\0\1",		'S',	play_input },
	{ "",			0,		play_check },
};
#undef O

static void cmd_play_free()
{
	struct cmd_play *p = x->cmd_data;
	ffmem_free(p->audio_module);
	ffmem_free(p);
}

static struct ffarg_ctx cmd_play_init(void *obj)
{
	x->cmd_data = ffmem_new(struct cmd_play);
	x->cmd_free = cmd_play_free;
	struct ffarg_ctx cx = {
		cmd_play, x->cmd_data
	};
	return cx;
}
