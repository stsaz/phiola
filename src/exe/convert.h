/** phiola: executor: 'convert' command
2023, Simon Zolin */

static int conv_help()
{
	help_info_write("\
Convert audio:\n\
    `phiola convert` INPUT... -o OUTPUT [OPTIONS]\n\
\n\
INPUT                   File name, directory or URL\n\
                          `@stdin`  Read from standard input\n\
                          `@names`  Read file names from standard input\n\
\n\
Options:\n\
  `-include` WILDCARD     Only include files matching a wildcard (case-insensitive)\n\
  `-exclude` WILDCARD     Exclude files & directories matching a wildcard (case-insensitive)\n\
\n\
  `-tracks` NUMBER[,...]  Select only specific tracks in a .cue list\n\
\n\
  `-cue_gaps` STRING      Control track pregaps:\n\
                          `skip`      Skip pregaps, e.g.:\n\
                                      track01.index01 .. track02.index00\n\
                          `current`   Add gap to the beginning of the current track, e.g.:\n\
                                      track01.index00 .. track02.index00\n\
                          `previous`  Add gap to the previous track,\n\
                                     but track01's pregap is preserved, e.g.:\n\
                                      track01.index00 .. track02.index01\n\
                                      track02.index01 .. track03.index01\n\
                        By default, adds gap to the previous track, e.g.:\n\
                                      track01.index01 .. track02.index01\n\
\n\
  `-seek` TIME            Seek to time: [[HH:]MM:]SS[.MSC]\n\
  `-until` TIME           Stop at time\n\
\n\
  `-aformat` FORMAT       Audio sample format:\n\
                          int8 | int16 | int24 | int32 | float32\n\
  `-rate` NUMBER          Sample rate\n\
  `-channels` NUMBER      Channels number\n\
  `-danorm` \"OPTIONS\"     Apply Dynamic Audio Normalizer filter. Options:\n\
                          `frame`       Integer\n\
                          `size`        Integer\n\
                          `peak`        Float\n\
                          `max-amp`     Float\n\
                          `target-rms`  Float\n\
                          `compress`    Float\n\
  `-gain` NUMBER          Gain/attenuation in dB\n\
\n\
  `-copy`                 Copy audio data without re-encoding\n\
  `-aac_profile` STRING   AAC profile:\n\
                          `LC`  AAC-LC\n\
                          `HE`  AAC-HE\n\
                          `HE2` AAC-HEv2\n\
  `-aac_quality` NUMBER   AAC encoding quality:\n\
                          1..5 (VBR) or 8..800 (CBR, kbit/s)\n\
  `-aac_bandwidth` NUMBER\n\
                          AAC cut-off frequency, Hz (<=20000)\n\
  `-opus_quality` NUMBER  Opus encoding bitrate:\n\
                          6..510 (VBR)\n\
  `-opus_mode` CHAR       Opus mode:\n\
                          `a`  Audio (default)\n\
                          `v`  VOIP\n\
  `-vorbis_quality` NUMBER\n\
                        Vorbis encoding quality:\n\
                          0..10\n\
  `-mp3_quality` NUMBER   MP3 encoding quality:\n\
                          9..0 (VBR) or 64..320 (CBR, kbit/s)\n\
\n\
  `-meta` NAME=VALUE      Meta data\n\
                          .mp4 supports: album, albumartist, artist, comment, composer, copyright, date, discnumber, genre, lyrics, title, tracknumber.\n\
\n\
  `-out` FILE             Output file name.\n\
                          `@stdout`    Write to standard output\n\
                        The encoder is selected automatically from the given file extension:\n\
                          .m4a       AAC\n\
                          .ogg       Vorbis\n\
                          .opus      Opus\n\
                          .flac      FLAC\n\
                          .wav       PCM\n\
                        Supports runtime variable expansion:\n\
                          `@filepath`  Expands to the input file path\n\
                          `@filename`  Input file name (without extension)\n\
                          `@nowdate`   Current date\n\
                          `@nowtime`   Current time\n\
                          `@counter`   Sequentially incremented number\n\
                          @STRING    Expands to file meta data,\n\
                                       e.g. `-o \"@tracknumber. @artist - @title.flac\"`\n\
                        When file name isn't specified, @filename is used automatically,\n\
                          e.g. `-o .ogg` == `-o @filename.ogg`\n\
  `-force`                Overwrite output file\n\
  `-preserve_date`        Preserve file modification date\n\
\n\
  `-workers` N            Max. number of workers\n\
  `-cpu_affinity` STRING  Set Worker-CPU affinity:\n\
                          `auto`\n\
  `-perf`                 Print performance counters\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_conv {
	const char*	aac_profile;
	const char*	opus_mode;
	const char*	danorm;
	const char*	output;
	ffvec	include, exclude; // ffstr[]
	ffvec	input; // ffstr[]
	ffvec	meta;
	ffvec	tracks; // uint[]
	int		gain;
	u_char	copy;
	u_char	cue_gaps;
	u_char	perf;
	uint	aac_bandwidth;
	uint	aac_q;
	uint	aformat;
	uint	channels;
	uint	force;
	uint	mp3_q;
	uint	opus_mode_n;
	uint	opus_q;
	uint	preserve_date;
	uint	rate;
	uint	vorbis_q;
	uint64	seek;
	uint64	until;

	u_char	aenc;
};

static int conv_include(struct cmd_conv *v, ffstr s)
{
	*ffvec_pushT(&v->include, ffstr) = s;
	return 0;
}

static int conv_exclude(struct cmd_conv *v, ffstr s)
{
	*ffvec_pushT(&v->exclude, ffstr) = s;
	return 0;
}

static int conv_aformat(struct cmd_conv *v, ffstr s)
{
	if (~0U == (v->aformat = phi_af_val(s)))
		return _ffargs_err(&x->cmd, 1, "incorrect audio format '%S'", &s);
	return 0;
}

static int conv_meta(struct cmd_conv *v, ffstr s)
{
	ffstr name, val;
	if (ffstr_splitby(&s, '=', &name, &val) <= 0)
		return _ffargs_err(&x->cmd, 1, "invalid meta: '%S'", &s);
	*ffvec_pushT(&v->meta, ffstr) = s;
	return 0;
}

static int conv_tracks(struct cmd_conv *v, ffstr s) { return cmd_tracks(&v->tracks, s); }

static int conv_input(struct cmd_conv *v, ffstr s)
{
	x->stdin_busy = ffstr_eqz(&s, "@stdin");
	return cmd_input(&v->input, s);
}

static int conv_cue_gaps(struct cmd_conv *v, ffstr s)
{
	static const char values[][8] = {
		"current",
		"previous",
		"skip",
	};
	static const u_char numbers[] = {
		PHI_CUE_GAP_CURR,
		PHI_CUE_GAP_PREV1,
		PHI_CUE_GAP_SKIP,
	};
	int r = ffcharr_findsorted(values, FF_COUNT(values), sizeof(values[0]), s.ptr, s.len);
	if (r < 0)
		return _ffargs_err(&x->cmd, 1, "-cue_gaps: value '%S' is not recognized", &s);
	v->cue_gaps = numbers[r];
	return 0;
}

static int conv_seek(struct cmd_conv *v, ffstr s) { return cmd_time_value(&v->seek, s); }

static int conv_until(struct cmd_conv *v, ffstr s) { return cmd_time_value(&v->until, s); }

static int conv_workers(struct cmd_conv *v, uint64 val)
{
	x->workers = val;
	return 0;
}

static int conv_cpu_affinity(struct cmd_conv *v, ffstr s)
{
	if (ffstr_eqz(&s, "auto"))
		x->cpu_affinity = ~0U;
	else
		return _ffargs_err(&x->cmd, 1, "-cpu_affinity: incorrect value: '%S'", &s);
	return 0;
}

static int conv_action(struct cmd_conv *v)
{
	x->queue->on_change(q_on_change);

	struct phi_track_conf c = {
		.ifile = {
			.include = *(ffslice*)&v->include,
			.exclude = *(ffslice*)&v->exclude,
			.preserve_date = v->preserve_date,
		},
		.cue_gaps = v->cue_gaps,
		.tracks = *(ffslice*)&v->tracks,
		.seek_msec = v->seek,
		.until_msec = v->until,
		.afilter = {
			.gain_db = v->gain,
			.danorm = v->danorm,
		},
		.oaudio = {
			.format = {
				.format = v->aformat,
				.rate = v->rate,
				.channels = v->channels,
			},
		},
		.ofile = {
			.name = ffsz_dup(v->output),
			.overwrite = v->force,
		},
		.stream_copy = v->copy,
		.print_time = v->perf,
		.cross_worker_assign = 1,
	};

	switch (v->aenc) {
	case PHI_AC_AAC:
		c.aac.profile = v->aac_profile[0];
		c.aac.quality = v->aac_q;
		c.aac.bandwidth = (ushort)v->aac_bandwidth;
		break;

	case PHI_AC_OPUS:
		c.opus.bitrate = v->opus_q;
		c.opus.mode = v->opus_mode_n;
		break;

	case PHI_AC_MP3:
		c.mp3.quality = (v->mp3_q != ~0U) ? (v->mp3_q + 1) : 0;  break;

	case PHI_AC_VORBIS:
		c.vorbis.quality = (v->vorbis_q) ? (v->vorbis_q + 1) * 10 : 0;  break;
	}

	cmd_meta_set(&c.meta, &v->meta);
	ffvec_free(&v->meta);

	struct phi_queue_conf qc = {
		.first_filter = &phi_guard,
		.ui_module = "tui.play",
		.tconf = c,
		.conversion = 1,
	};
	x->queue->create(&qc);
	ffstr *it;
	FFSLICE_WALK(&v->input, it) {
		struct phi_queue_entry qe = {
			.url = it->ptr,
		};
		x->queue->add(NULL, &qe);
	}
	ffvec_free(&v->input);

	x->queue->play(NULL, NULL);
	return 0;
}

static int conv_prepare(struct cmd_conv *v)
{
	if (!v->input.len)
		return _ffargs_err(&x->cmd, 1, "please specify input file");

	if (!v->output)
		return _ffargs_err(&x->cmd, 1, "please specify output file name with '-out FILE'");

	if (!(v->aac_profile = cmd_aac_profile(v->aac_profile)))
		return _ffargs_err(&x->cmd, 1, "-aac_profile: incorrect value");

	if ((int)(v->opus_mode_n = cmd_opus_mode(v->opus_mode)) < 0)
		return _ffargs_err(&x->cmd, 1, "-opus_mode: incorrect value");

	ffstr dir, name, ext;
	ffpath_split3_output(FFSTR_Z(v->output), &dir, &name, &ext);
	x->stdout_busy = (!dir.len && ffstr_eqz(&name, "@stdout"));
	if (!ext.len)
		return _ffargs_err(&x->cmd, 1, "Please specify output file extension: \"%s\"", v->output);
	if (!(v->aenc = cmd_oext_aenc(ext, v->copy)))
		return _ffargs_err(&x->cmd, 1, "Specified output file format is not supported: \"%S\"", &ext);
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_conv, m)
static const struct ffarg cmd_conv[] = {
	{ "-aac_bandwidth",	'u',	O(aac_bandwidth) },
	{ "-aac_profile",	's',	O(aac_profile) },
	{ "-aac_quality",	'u',	O(aac_q) },
	{ "-aformat",		'S',	conv_aformat },
	{ "-channels",		'u',	O(channels) },
	{ "-copy",			'1',	O(copy) },
	{ "-cpu_affinity",	'S',	conv_cpu_affinity },
	{ "-cue_gaps",		'S',	conv_cue_gaps },
	{ "-danorm",		's',	O(danorm) },
	{ "-exclude",		'+S',	conv_exclude },
	{ "-force",			'1',	O(force) },
	{ "-gain",			'd',	O(gain) },
	{ "-help",			0,		conv_help },
	{ "-include",		'+S',	conv_include },
	{ "-m",				'+S',	conv_meta },
	{ "-meta",			'+S',	conv_meta },
	{ "-mp3_quality",	'u',	O(mp3_q) },
	{ "-o",				's',	O(output) },
	{ "-opus_mode",		's',	O(opus_mode) },
	{ "-opus_quality",	'u',	O(opus_q) },
	{ "-out",			's',	O(output) },
	{ "-perf",			'1',	O(perf) },
	{ "-preserve_date",	'1',	O(preserve_date) },
	{ "-rate",			'u',	O(rate) },
	{ "-seek",			'S',	conv_seek },
	{ "-tracks",		'S',	conv_tracks },
	{ "-until",			'S',	conv_until },
	{ "-vorbis_quality",'u',	O(vorbis_q) },
	{ "-workers",		'u',	conv_workers },
	{ "\0\1",			'S',	conv_input },
	{ "",				0,		conv_prepare },
};
#undef O

static void cmd_conv_free(struct cmd_conv *v)
{
	ffvec_free(&v->include);
	ffvec_free(&v->exclude);
	ffmem_free(v);
}

static struct ffarg_ctx cmd_conv_init(void *obj)
{
	struct cmd_conv *v = ffmem_new(struct cmd_conv);
	v->mp3_q = ~0U;
	return SUBCMD_INIT(v, cmd_conv_free, conv_action, cmd_conv);
}
