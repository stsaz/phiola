/** phiola: executor: 'record' command
2023, Simon Zolin */

static int rec_help()
{
	help_info_write("\
Record audio:\n\
    `phiola record` -o OUTPUT [OPTIONS]\n\
\n\
Options:\n\
  `-audio` STRING         Audio library name (e.g. alsa)\n\
  `-device` NUMBER        Capture device number\n\
  `-exclusive`            Open device in exclusive mode (WASAPI)\n\
  `-loopback`             Loopback mode (\"record what you hear\") (WASAPI)\n\
                          Note: '-device NUMBER' specifies Playback device and not Capture device.\n\
  `-buffer` NUMBER        Length (in msec) of the capture buffer\n\
  `-aformat` FORMAT       Audio sample format:\n\
                          int8 | int16 | int24 | int32 | float32\n\
  `-rate` NUMBER          Sample rate\n\
  `-channels` NUMBER      Channels number\n\
\n\
  `-until` TIME           Stop at time\n\
                          [[HH:]MM:]SS[.MSC]\n\
\n\
  `-danorm` \"OPTIONS\"     Apply Dynamic Audio Normalizer filter. Options:\n\
                          `frame`       Integer\n\
                          `size`        Integer\n\
                          `peak`        Float\n\
                          `max-amp`     Float\n\
                          `target-rms`  Float\n\
                          `compress`    Float\n\
  `-gain` NUMBER          Gain/attenuation in dB\n\
\n\
  `-aac_profile` CHAR     AAC profile:\n\
                          `l`  AAC-LC\n\
                          `h`  AAC-HE\n\
                          `H`  AAC-HEv2\n\
  `-aac_quality` NUMBER   AAC encoding quality:\n\
                          1..5 (VBR) or 8..800 (CBR, kbit/s)\n\
  `-opus_quality` NUMBER  Opus encoding bitrate:\n\
                          6..510 (VBR)\n\
  `-vorbis_quality` NUMBER\n\
                        Vorbis encoding quality:\n\
                          0..10\n\
\n\
  `-meta` NAME=VALUE      Meta data\n\
                          .mp3 supports: album, albumartist, artist, comment, date, genre, picture, publisher, title, tracknumber, tracktotal.\n\
                          .mp4 supports: album, albumartist, artist, comment, composer, copyright, date, discnumber, genre, lyrics, title, tracknumber.\n\
                          .flac, .ogg support tags of any name, but the use of MP3/MP4-compatible names is recommended.\n\
\n\
  `-out` FILE             Output file name\n\
                          `@stdout`    Write to standard output\n\
                        The encoder is selected automatically from the given file extension:\n\
                          .m4a       AAC\n\
                          .ogg       Vorbis\n\
                          .opus      Opus\n\
                          .flac      FLAC\n\
                          .wav       PCM\n\
                        Supports runtime variable expansion:\n\
                          `@nowdate`   Current date\n\
                          `@nowtime`   Current time\n\
                          `@counter`   Sequentially incremented number\n\
  `-force`                Overwrite output file\n\
\n\
  `-remote`               Listen for incoming remote commands\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_rec {
	char*	audio_module;
	const char*	aac_profile;
	const char*	audio;
	const char*	danorm;
	const char*	output;
	ffvec	meta;
	int		gain;
	u_char	exclusive;
	u_char	force;
	u_char	loopback;
	u_char	remote;
	uint	aac_q;
	uint	aformat;
	uint	buffer;
	uint	channels;
	uint	device;
	uint	opus_q;
	uint	rate;
	uint	vorbis_q;
	uint64	until;
};

#ifdef FF_WIN
/** Start the track that generates silence so that
 WASAPI loopback recording won't be paused when there's nothing else playing. */
static void rec_silence_track(struct cmd_rec *r)
{
	const phi_track_if *track = x->core->track;
	struct phi_track_conf c = {
		.oaudio = {
			.device_index = r->device,
		},
	};
	phi_track *t = track->create(&c);

	struct phi_af af = {
		.format = PHI_PCM_16,
		.rate = 48000,
		.channels = 2,
	};
	t->oaudio.format = af;

	if (!track->filter(t, x->core->mod("afilter.silence-gen"), 0)
		|| !track->filter(t, x->core->mod("wasapi.play"), 0)) {
		track->close(t);
		return;
	}

	track->start(t);
}

#else
static void rec_silence_track(struct cmd_rec *r) {}
#endif

static int rec_action(struct cmd_rec *r)
{
	struct phi_track_conf c = {
		.iaudio = {
			.format = {
				.format = r->aformat,
				.rate = r->rate,
				.channels = r->channels,
			},
			.device_index = r->device,
			.exclusive = r->exclusive,
			.loopback = r->loopback,
			.buf_time = r->buffer,
		},
		.until_msec = r->until,
		.afilter = {
			.gain_db = r->gain,
			.danorm = r->danorm,
		},
		.aac = {
			.profile = r->aac_profile[0],
			.quality = r->aac_q,
		},
		.vorbis.quality = (r->vorbis_q + 1) * 10,
		.opus = {
			.bitrate = r->opus_q,
		},
		.ofile = {
			.name = ffsz_dup(r->output),
			.overwrite = r->force,
		},
	};
	const phi_track_if *track = x->core->track;
	phi_track *t = track->create(&c);

	const char *input = "core.auto-rec";
	if (r->audio) {
		r->audio_module = ffsz_allocfmt("%s.rec%Z", r->audio);
		input = r->audio_module;
	}

	const char *output = (x->stdout_busy) ? "core.stdout" : "core.file-write";

	if (!track->filter(t, &phi_guard, 0)
		|| !track->filter(t, x->core->mod(input), 0)
		|| !track->filter(t, x->core->mod("afilter.until"), 0)
		|| !track->filter(t, x->core->mod("afilter.rtpeak"), 0)
		|| !track->filter(t, x->core->mod("tui.rec"), 0)
		|| (r->danorm
			&& !track->filter(t, x->core->mod("danorm.f"), 0))
		|| !track->filter(t, x->core->mod("afilter.gain"), 0)
		|| !track->filter(t, x->core->mod("afilter.auto-conv"), 0)
		|| !track->filter(t, x->core->mod("format.auto-write"), 0)
		|| !track->filter(t, x->core->mod(output), 0)) {
		track->close(t);
		return -1;
	}

	cmd_meta_set(&t->meta, &r->meta);
	ffvec_free(&r->meta);

	x->mode_record = 1;
	track->start(t);

	if (r->remote) {
		const phi_remote_sv_if *rsv = x->core->mod("remote.server");
		if (rsv->start(NULL))
			return -1;
	}

	if (r->loopback)
		rec_silence_track(r);

	return 0;
}

static int rec_check(struct cmd_rec *r)
{
	if (!r->output)
		return _ffargs_err(&x->cmd, 1, "please specify output file name with '-out FILE'");
	if (!r->aac_profile)
		r->aac_profile = "l";

	ffstr name;
	ffpath_splitname_str(FFSTR_Z(r->output), &name, NULL);
	x->stdout_busy = ffstr_eqz(&name, "@stdout");
	return 0;
}

static int rec_meta(struct cmd_rec *r, ffstr s)
{
	ffstr name, val;
	if (ffstr_splitby(&s, '=', &name, &val) <= 0)
		return _ffargs_err(&x->cmd, 1, "invalid meta: '%S'", &s);
	*ffvec_pushT(&r->meta, ffstr) = s;
	return 0;
}

static int rec_aformat(struct cmd_rec *r, ffstr s)
{
	if (~0U == (r->aformat = phi_af_val(s)))
		return _ffargs_err(&x->cmd, 1, "incorrect audio format '%S'", &s);
	return 0;
}

static int rec_until(struct cmd_rec *r, ffstr s) { return cmd_time_value(&r->until, s); }

#define O(m)  (void*)FF_OFF(struct cmd_rec, m)
static const struct ffarg cmd_rec[] = {
	{ "-aac_profile",	's',	O(aac_profile) },
	{ "-aac_quality",	'u',	O(aac_q) },
	{ "-aformat",		'S',	rec_aformat },
	{ "-audio",			's',	O(audio) },
	{ "-buffer",		'u',	O(buffer) },
	{ "-channels",		'u',	O(channels) },
	{ "-danorm",		's',	O(danorm) },
	{ "-device",		'u',	O(device) },
	{ "-exclusive",		'1',	O(exclusive) },
	{ "-force",			'1',	O(force) },
	{ "-gain",			'd',	O(gain) },
	{ "-help",			0,		rec_help },
	{ "-loopback",		'1',	O(loopback) },
	{ "-meta",			'+S',	rec_meta },
	{ "-o",				's',	O(output) },
	{ "-opus_quality",	'u',	O(opus_q) },
	{ "-out",			's',	O(output) },
	{ "-rate",			'u',	O(rate) },
	{ "-remote",		'1',	O(remote) },
	{ "-until",			'S',	rec_until },
	{ "-vorbis_quality",'u',	O(vorbis_q) },
	{ "",				0,		rec_check },
};
#undef O

static void cmd_rec_free(struct cmd_rec *r)
{
	ffmem_free(r->audio_module);
	ffmem_free(r);
}

static struct ffarg_ctx cmd_rec_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct cmd_rec), cmd_rec_free, rec_action, cmd_rec);
}
