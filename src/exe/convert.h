/** phiola: executor: 'convert' command
2023, Simon Zolin */

static int conv_help()
{
	static const char s[] = "\
Convert audio:\n\
    phiola convert INPUT... -o OUTPUT\n\
\n\
INPUT                   File name, directory or URL\n\
                          @stdin  Read from standard input\n\
\n\
Options:\n\
  -include WILDCARD     Only include files matching a wildcard (case-insensitive)\n\
  -exclude WILDCARD     Exclude files & directories matching a wildcard (case-insensitive)\n\
\n\
  -tracks NUMBER[,...]  Select only specific tracks in a .cue list\n\
\n\
  -seek TIME            Seek to time: [[HH:]MM:]SS[.MSC]\n\
  -until TIME           Stop at time\n\
\n\
  -aformat FORMAT       Audio sample format:\n\
                          int8 | int16 | int24 | int32 | float32\n\
  -rate NUMBER          Sample rate\n\
  -channels NUMBER      Channels number\n\
  -danorm \"OPTIONS\"     Apply Dynamic Audio Normalizer filter. Options:\n\
                          frame       Integer\n\
                          size        Integer\n\
                          peak        Float\n\
                          max-amp     Float\n\
                          target-rms  Float\n\
                          compress    Float\n\
  -gain NUMBER          Gain/attenuation in dB\n\
\n\
  -copy                 Copy audio data without re-encoding\n\
  -aac-profile CHAR     AAC profile:\n\
                          l  AAC-LC\n\
                          h  AAC-HE\n\
                          H  AAC-HEv2\n\
  -aac-quality NUMBER   AAC encoding quality:\n\
                          1..5 (VBR) or 8..800 (CBR, kbit/s)\n\
  -opus-quality NUMBER  Opus encoding bitrate:\n\
                          6..510 (VBR)\n\
  -vorbis-quality NUMBER\n\
                        Vorbis encoding quality:\n\
                          -1..10\n\
\n\
  -meta STRING=STRING   Meta data\n\
\n\
  -out FILE             Output file name.\n\
                          @stdout    Write to standard output\n\
                        The encoder is selected automatically from the given file extension:\n\
                          .m4a       AAC\n\
                          .ogg       Vorbis\n\
                          .opus      Opus\n\
                          .flac      FLAC\n\
                          .wav       PCM\n\
                        Supports runtime variable expansion:\n\
                          @filepath  Expands to the input file path\n\
                          @filename  Input file name (without extension)\n\
                          @nowdate   Current date\n\
                          @nowtime   Current time\n\
                          @counter   Sequentially incremented number\n\
                          @STRING    Expands to file meta data,\n\
                                       e.g. `-o \"@tracknumber. @artist - @title.flac\"`\n\
                        When file name isn't specified, @filepath is used automatically,\n\
                          e.g. `-o .ogg` == `-o @filename.ogg`\n\
  -force                Overwrite output file\n\
  -preserve-date        Preserve file modification date\n\
";
	ffstdout_write(s, FFS_LEN(s));
	x->exit_code = 0;
	return 1;
}

struct cmd_conv {
	ffvec input; // ffstr[]
	ffvec include, exclude; // ffstr[]
	ffvec meta;
	uint aformat;
	uint channels;
	uint rate;
	uint64 seek;
	uint64 until;
	int gain;
	ffbyte copy;
	const char *danorm;
	char *aac_profile;
	uint aac_q;
	uint opus_q;
	uint vorbis_q;
	char *output;
	uint force;
	uint preserve_date;
	ffvec tracks; // uint[]
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
	if (~0U == (v->aformat = pcm_str_fmt(s.ptr, s.len)))
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
	if (s.len && s.ptr[0] == '-')
		return _ffargs_err(&x->cmd, 1, "unknown option '%S'. Use '-h' for usage info.", &s);

	x->stdin_busy = ffstr_eqz(&s, "@stdin");
	*ffvec_pushT(&v->input, ffstr) = s;
	return 0;
}

static int conv_seek(struct cmd_conv *v, ffstr s) { return cmd_time_value(&v->seek, s); }

static int conv_until(struct cmd_conv *v, ffstr s) { return cmd_time_value(&v->until, s); }

static void conv_qu_add(struct cmd_conv *v, ffstr *fn)
{
	struct phi_track_conf c = {
		.ifile = {
			.name = ffsz_dupstr(fn),
			.include = *(ffslice*)&v->include,
			.exclude = *(ffslice*)&v->exclude,
			.preserve_date = v->preserve_date,
		},
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
		.aac = {
			.profile = v->aac_profile[0],
			.quality = v->aac_q,
		},
		.vorbis.quality = v->vorbis_q,
		.opus = {
			.bitrate = v->opus_q,
		},
		.ofile = {
			.name = ffsz_dup(v->output),
			.overwrite = v->force,
		},
		.stream_copy = v->copy,
	};

	cmd_meta_set(&c.meta, &v->meta);
	ffvec_free(&v->meta);

	struct phi_queue_entry qe = {
		.conf = c,
	};
	x->queue->add(NULL, &qe);
}

static int conv_action()
{
	struct cmd_conv *v = x->cmd_data;

	struct phi_queue_conf qc = {
		.name = "convert",
		.first_filter = &phi_guard,
		.ui_module = "tui.play",
		.conversion = 1,
	};
	x->queue->create(&qc);
	ffstr *it;
	FFSLICE_WALK(&v->input, it) {
		conv_qu_add(v, it);
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
	if (!v->aac_profile)
		v->aac_profile = "l";

	ffstr name;
	ffpath_splitname_str(FFSTR_Z(v->output), &name, NULL);
	x->stdout_busy = ffstr_eqz(&name, "@stdout");

	x->action = conv_action;
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_conv, m)
static const struct ffarg cmd_conv[] = {
	{ "-aac-profile",	's',	O(aac_profile) },
	{ "-aac-quality",	'u',	O(aac_q) },
	{ "-aformat",		'S',	conv_aformat },
	{ "-channels",		'u',	O(channels) },
	{ "-copy",			'1',	O(copy) },
	{ "-danorm",		's',	O(danorm) },
	{ "-exclude",		'S',	conv_exclude },
	{ "-force",			'1',	O(force) },
	{ "-gain",			'd',	O(gain) },
	{ "-help",			0,		conv_help },
	{ "-include",		'S',	conv_include },
	{ "-meta",			'+S',	conv_meta },
	{ "-o",				's',	O(output) },
	{ "-opus-quality",	'u',	O(opus_q) },
	{ "-out",			's',	O(output) },
	{ "-preserve-date",	'1',	O(preserve_date) },
	{ "-rate",			'u',	O(rate) },
	{ "-seek",			'S',	conv_seek },
	{ "-tracks",		'S',	conv_tracks },
	{ "-until",			'S',	conv_until },
	{ "-vorbis-quality",'u',	O(vorbis_q) },
	{ "\0\1",			'S',	conv_input },
	{ "",				0,		conv_prepare },
};
#undef O

static struct ffarg_ctx cmd_conv_init(void *obj)
{
	x->cmd_data = ffmem_new(struct cmd_conv);
	struct ffarg_ctx cx = {
		cmd_conv, x->cmd_data
	};
	return cx;
}
