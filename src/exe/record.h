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
  `-split` TIME           Create new output file periodically\n\
                          [[HH:]MM:]SS[.MSC]\n\
  `-until` TIME           Stop at time\n\
                          [[HH:]MM:]SS[.MSC]\n\
\n\
  `-noise_gate` \"OPTIONS\" Suppress noise. Options:\n\
                          `threshold`   Integer (dB)\n\
                          `release`     Integer (msec)\n\
  `-danorm` \"OPTIONS\"     Apply Dynamic Audio Normalizer filter. Options:\n\
                          `frame`       Integer\n\
                          `size`        Integer\n\
                          `peak`        Float\n\
                          `max-amp`     Float\n\
                          `target-rms`  Float\n\
                          `compress`    Float\n\
  `-gain` NUMBER          Gain/attenuation in dB\n\
\n\
  `-aac_profile` STRING   AAC profile:\n\
                          `LC`  AAC-LC\n\
                          `HE`  AAC-HE\n\
                          `HE2` AAC-HEv2\n\
  `-aac_quality` NUMBER   AAC encoding quality:\n\
                          1..5 (VBR) or 8..800 (CBR, kbit/s)\n\
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
  `-remote_id` STRING     phiola instance ID\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_rec {
	char*	audio_module;
	const char*	aac_profile;
	const char*	audio;
	const char*	danorm;
	const char*	noise_gate;
	const char*	opus_mode;
	const char*	output;
	const char*	remote_id;
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
	uint	mp3_q;
	uint	opus_mode_n;
	uint	opus_q;
	uint	rate;
	uint	split;
	uint	vorbis_q;
	uint64	until;

	u_char	aenc;
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

static void rec_grd_close(void *f, phi_track *t)
{
	phi_grd_close(f, t);

	ffmem_free(t->conf.ofile.name);  t->conf.ofile.name = NULL;
	x->core->metaif->destroy(&t->meta);
	x->core->sig(PHI_CORE_STOP);
}

static const phi_filter rec_guard = {
	NULL, rec_grd_close, phi_grd_process,
	"rec-guard"
};

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
		.split_msec = r->split,
		.until_msec = r->until,
		.afilter = {
			.gain_db = r->gain,
			.danorm = r->danorm,
			.noise_gate = r->noise_gate,
		},
		.oaudio = {
			.format = {
				.format = r->aformat,
				.rate = r->rate,
				.channels = r->channels,
			},
		},
		.ofile = {
			.name = ffsz_dup(r->output),
			.overwrite = r->force,
		},
	};

	switch (r->aenc) {
	case PHI_AC_AAC:
		c.aac.profile = r->aac_profile[0];
		c.aac.quality = r->aac_q;
		break;

	case PHI_AC_OPUS:
		c.opus.bitrate = r->opus_q;
		c.opus.mode = r->opus_mode_n;
		break;

	case PHI_AC_MP3:
		c.mp3.quality = (r->mp3_q != ~0U) ? (r->mp3_q + 1) : 0;  break;

	case PHI_AC_VORBIS:
		c.vorbis.quality = (r->vorbis_q) ? (r->vorbis_q + 1) * 10 : 0;  break;
	}

	const phi_track_if *track = x->core->track;
	phi_track *t = track->create(&c);

	const char *input = "core.auto-rec";
	if (r->audio) {
		r->audio_module = ffsz_allocfmt("%s.rec%Z", r->audio);
		input = r->audio_module;
	}

	const char *output = (x->stdout_busy) ? "core.stdout" : "core.file-write";

	if (!track->filter(t, &rec_guard, 0)
		|| !track->filter(t, x->core->mod(input), 0)
		|| !track->filter(t, x->core->mod("afilter.until"), 0)
		|| !track->filter(t, x->core->mod("afilter.rtpeak"), 0)
		|| !track->filter(t, x->core->mod("tui.rec"), 0)
		|| (r->noise_gate
			&& !track->filter(t, x->core->mod("afilter.noise-gate"), 0))
		|| (r->danorm
			&& !track->filter(t, x->core->mod("af-danorm.f"), 0))
		|| !track->filter(t, x->core->mod("afilter.gain"), 0)
		|| !track->filter(t, x->core->mod("afilter.auto-conv"), 0)
		|| (r->split
			&& !track->filter(t, x->core->mod("afilter.split"), 0))
		|| (!r->split
			&& (!track->filter(t, x->core->mod("format.auto-write"), 0)
				|| !track->filter(t, x->core->mod(output), 0)))
		) {
		track->close(t);
		return -1;
	}

	cmd_meta_set(&t->meta, &r->meta);
	ffvec_free(&r->meta);

	t->output.allow_async = 1;
	track->start(t);

	if (r->remote || r->remote_id) {
		const phi_remote_sv_if *rsv = x->core->mod("remote.server");
		if (rsv->start(r->remote_id))
			return -1;
	}

	if (r->loopback)
		rec_silence_track(r);

	return 0;
}

static int rec_check(struct cmd_rec *r)
{
	if (r->noise_gate && r->danorm)
		return _ffargs_err(&x->cmd, 1, "`-noise_gate` and `-danorm` can't be used together");

	if (!r->output)
		return _ffargs_err(&x->cmd, 1, "please specify output file name with '-out FILE'");

	if (!(r->aac_profile = cmd_aac_profile(r->aac_profile)))
		return _ffargs_err(&x->cmd, 1, "-aac_profile: incorrect value");

	if ((int)(r->opus_mode_n = cmd_opus_mode(r->opus_mode)) < 0)
		return _ffargs_err(&x->cmd, 1, "-opus_mode: incorrect value");

	ffstr name, ext;
	ffpath_splitname_str(FFSTR_Z(r->output), &name, &ext);
	x->stdout_busy = ffstr_eqz(&name, "@stdout");
	if (!ext.len)
		return _ffargs_err(&x->cmd, 1, "Please specify output file extension: \"%s\"", r->output);
	if (!(r->aenc = cmd_oext_aenc(ext, 0)))
		return _ffargs_err(&x->cmd, 1, "Specified output file format is not supported: \"%S\"", &ext);

	if (r->buffer)
		x->timer_int_msec = ffmin(r->buffer / 2, x->timer_int_msec);
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

static int rec_split(struct cmd_rec *c, ffstr s)
{
	uint64 v;
	int r;
	if ((r = cmd_time_value(&v, s)))
		return r;
	c->split = v;
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
	{ "-m",				'+S',	rec_meta },
	{ "-meta",			'+S',	rec_meta },
	{ "-mp3_quality",	'u',	O(mp3_q) },
	{ "-noise_gate",	's',	O(noise_gate) },
	{ "-o",				's',	O(output) },
	{ "-opus_mode",		's',	O(opus_mode) },
	{ "-opus_quality",	'u',	O(opus_q) },
	{ "-out",			's',	O(output) },
	{ "-rate",			'u',	O(rate) },
	{ "-remote",		'1',	O(remote) },
	{ "-remote_id",		's',	O(remote_id) },
	{ "-split",			'S',	rec_split },
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
	struct cmd_rec *r = ffmem_new(struct cmd_rec);
	r->mp3_q = ~0U;
	return SUBCMD_INIT(r, cmd_rec_free, rec_action, cmd_rec);
}
