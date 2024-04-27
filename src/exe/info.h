/** phiola: executor: 'info' command
2023, Simon Zolin */

static int info_help()
{
	help_info_write("\
Show file meta data:\n\
    `phiola info` [OPTIONS] INPUT...\n\
\n\
INPUT                   File name, directory or URL\n\
                          `@stdin`  Read from standard input\n\
\n\
Options:\n\
  `-include` WILDCARD     Only include files matching a wildcard (case-insensitive)\n\
  `-exclude` WILDCARD     Exclude files & directories matching a wildcard (case-insensitive)\n\
\n\
  `-tags`                 Print all meta tags\n\
\n\
  `-tracks` NUMBER[,...]  Select only specific tracks in a .cue list\n\
\n\
  `-seek` TIME            Seek to time:\n\
                          [[HH:]MM:]SS[.MSC]\n\
  `-until` TIME           Stop at time\n\
\n\
  `-peaks`                Analyze audio and print some details\n\
  `-peaks_crc`            Print audio data CRC (use with `-peaks`)\n\
\n\
  `-perf`                 Print performance counters\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_info {
	u_char	pcm_crc;
	u_char	pcm_peaks;
	u_char	perf;
	u_char	tags;
	ffvec	include, exclude; // ffstr[]
	ffvec	input; // ffstr[]
	ffvec	tracks; // uint[]
	uint64	seek;
	uint64	until;
};

static int info_seek(struct cmd_info *p, ffstr s) { return cmd_time_value(&p->seek, s); }

static int info_until(struct cmd_info *p, ffstr s) { return cmd_time_value(&p->until, s); }

static int info_include(struct cmd_info *p, ffstr s)
{
	*ffvec_pushT(&p->include, ffstr) = s;
	return 0;
}

static int info_exclude(struct cmd_info *p, ffstr s)
{
	*ffvec_pushT(&p->exclude, ffstr) = s;
	return 0;
}

static int info_tracks(struct cmd_info *p, ffstr s) { return cmd_tracks(&p->tracks, s); }

static int info_input(struct cmd_info *p, ffstr s)
{
	if (s.len && s.ptr[0] == '-')
		return _ffargs_err(&x->cmd, 1, "unknown option '%S'. Use '-h' for usage info.", &s);

	x->stdin_busy = ffstr_eqz(&s, "@stdin");
	return cmd_input(&p->input, s);
}

static void info_qu_add(struct cmd_info *p, ffstr *fn)
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
			.peaks_info = p->pcm_peaks,
			.peaks_crc = p->pcm_crc,
		},
		.info_only = !p->pcm_peaks,
		.print_tags = p->tags,
		.print_time = p->perf,
	};

	struct phi_queue_entry qe = {
		.conf = c,
	};
	x->queue->add(NULL, &qe);
}

static int info_action(struct cmd_info *p)
{
	x->queue->on_change(q_on_change);
	struct phi_queue_conf qc = {
		.first_filter = &phi_guard,
		.ui_module = "tui.play",
	};
	x->queue->create(&qc);
	ffstr *it;
	FFSLICE_WALK(&p->input, it) {
		info_qu_add(p, it);
	}
	ffvec_free(&p->input);

	x->queue->play(NULL, NULL);
	return 0;
}

static int info_check(struct cmd_info *p)
{
	if (!p->input.len)
		return _ffargs_err(&x->cmd, 1, "please specify input file");
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_info, m)
static const struct ffarg cmd_info[] = {
	{ "-exclude",	'S',	info_exclude },
	{ "-help",		0,		info_help },
	{ "-include",	'S',	info_include },
	{ "-peaks",		'1',	O(pcm_peaks) },
	{ "-peaks_crc",	'1',	O(pcm_crc) },
	{ "-perf",		'1',	O(perf) },
	{ "-seek",		'S',	info_seek },
	{ "-tags",		'1',	O(tags) },
	{ "-tracks",	'S',	info_tracks },
	{ "-until",		'S',	info_until },
	{ "\0\1",		'S',	info_input },
	{ "",			0,		info_check },
};
#undef O

static void cmd_info_free(struct cmd_info *p)
{
	ffvec_free(&p->include);
	ffvec_free(&p->exclude);
	ffmem_free(p);
}

static struct ffarg_ctx cmd_info_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct cmd_info), cmd_info_free, info_action, cmd_info);
}
