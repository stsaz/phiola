/** phiola: executor: 'device' command
2023, Simon Zolin */

static int dev_help()
{
	static const char s[] = "\
List audio devices:\n\
    phiola device list [OPTIONS]\n\
\n\
Options:\n\
  -audio STRING     Audio library name (e.g. alsa)\n\
  -capture          Show capture devices only\n\
  -playback         Show playback devices only\n\
";
	ffstdout_write(s, FFS_LEN(s));
	x->exit_code = 0;
	return 1;
}

struct cmd_dev_list {
	const char *audio;
	ffbyte capture;
	ffbyte playback;
};

static int dev_list_once(ffvec *buf, const phi_adev_if *adev, uint flags)
{
	struct phi_adev_ent *ents;
	uint ndev = adev->list(&ents, flags);
	if (ndev == ~0U)
		return -1;

	const char *title = (flags == PHI_ADEV_PLAYBACK) ? "Playback/Loopback" : "Capture";
	ffvec_addfmt(buf, "%s:\n", title);
	for (uint i = 0;  i != ndev;  i++) {

		ffstr def = {};
		if (ents[i].default_device)
			ffstr_setz(&def, " - Default");
		ffvec_addfmt(buf, "  %u: %s%S\n", i + 1, ents[i].name, &def);
	}

	adev->list_free(ents);
	return 0;
}

static const void* dev_find_mod()
{
	static const char mods[][20] = {
#if defined FF_WIN
		"wasapi.dev",
		"direct-sound.dev",

#elif defined FF_BSD
		"oss.dev",

#elif defined FF_APPLE
		"coreaudio.dev",

#elif defined FF_ANDROID
		"aaudio.dev",

#else
		"pulse.dev",
		"alsa.dev",
#endif
	};
	for (uint i = 0;  i < FF_COUNT(mods);  i++) {
		const void *f;
		if (NULL != (f = x->core->mod(mods[i])))
			return f;
	}

	return NULL;
}

static int dev_list_action()
{
	struct cmd_dev_list *l = x->cmd_data;
	char sbuf[1000];
	ffvec buf = {};

	const phi_adev_if *adev;
	if (l->audio) {
		ffsz_format(sbuf, sizeof(sbuf), "%s.dev", l->audio);
		adev = x->core->mod(sbuf);

	} else {
		adev = dev_find_mod();
	}
	if (!adev) return -1;

	uint f = 3;
	if (l->playback)
		f = 1;
	else if (l->capture)
		f = 2;

	if (f & 1)
		dev_list_once(&buf, adev, PHI_ADEV_PLAYBACK);
	if (f & 2)
		dev_list_once(&buf, adev, PHI_ADEV_CAPTURE);

	ffstdout_write(buf.ptr, buf.len);
	ffvec_free(&buf);

	x->core->sig(PHI_CORE_STOP);
	x->exit_code = 0;
	return 0;
}

static int dev_list_prepare()
{
	x->action = dev_list_action;
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_dev_list, m)
static const struct ffarg cmd_dev_list[] = {
	{ "-audio",		's',	O(audio) },
	{ "-capture",	'1',	O(capture) },
	{ "-help",		'1',	dev_help },
	{ "-playback",	'1',	O(playback) },
	{ "",			0,		dev_list_prepare },
};
#undef O

static struct ffarg_ctx cmd_dev_list_init()
{
	x->cmd_data = ffmem_new(struct cmd_dev_list);
	struct ffarg_ctx cx = {
		cmd_dev_list, x->cmd_data
	};
	return cx;
}

static const struct ffarg cmd_dev[] = {
	{ "-help",	'1',	dev_help },
	{ "list",	'{',	cmd_dev_list_init },
	{ "",		0,		dev_help },
};
