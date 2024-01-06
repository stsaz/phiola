/** phiola: executor: 'device' command
2023, Simon Zolin */

static int dev_help()
{
	help_info_write("\
List audio devices:\n\
    `phiola device list` [OPTIONS]\n\
\n\
Options:\n\
  `-audio` STRING     Audio library name (e.g. alsa)\n\
  `-capture`          Show capture devices only\n\
  `-playback`         Show playback devices only\n\
  `-filter` STRING    Filter devices by name\n\
  `-number`           Show only device number\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_dev_list {
	const char *audio;
	const char *filter;
	u_char capture;
	u_char number;
	u_char playback;
};

static int dev_list_once(struct cmd_dev_list *l, ffvec *buf, const phi_adev_if *adev, uint flags)
{
	struct phi_adev_ent *ents;
	uint ndev = adev->list(&ents, flags);
	if (ndev == ~0U)
		return -1;

	if (!l->number) {
		const char *title = (flags == PHI_ADEV_PLAYBACK) ? "Playback/Loopback" : "Capture";
		ffvec_addfmt(buf, "%s:\n", title);
	}

	uint n = 0;
	for (uint i = 0;  i != ndev;  i++) {

		if (l->filter) {
			ffstr name = FFSTR_INITZ(ents[i].name);
			if (ffstr_ifindz(&name, l->filter) < 0)
				continue;
		}

		n++;

		if (l->number) {
			ffvec_addfmt(buf, "%u\n", i + 1);
			continue;
		}

		ffstr def = {};
		if (ents[i].default_device)
			ffstr_setz(&def, " - Default");
		ffvec_addfmt(buf, "  %u: %s%S\n", i + 1, ents[i].name, &def);
	}

	adev->list_free(ents);
	return n;
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

static int dev_list_action(struct cmd_dev_list *l)
{
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

	int rp = 1, rc = 1;
	if (f & 1)
		rp = dev_list_once(l, &buf, adev, PHI_ADEV_PLAYBACK);
	if (f & 2)
		rc = dev_list_once(l, &buf, adev, PHI_ADEV_CAPTURE);

	ffstdout_write(buf.ptr, buf.len);
	ffvec_free(&buf);

	x->core->sig(PHI_CORE_STOP);

	x->exit_code = 0;
	if (l->filter && rp <= 0 && rc <= 0)
		x->exit_code = 1;

	return 0;
}

static int dev_list_prepare()
{
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_dev_list, m)
static const struct ffarg cmd_dev_list[] = {
	{ "-audio",		's',	O(audio) },
	{ "-capture",	'1',	O(capture) },
	{ "-filter",	's',	O(filter) },
	{ "-help",		'1',	dev_help },
	{ "-number",	'1',	O(number) },
	{ "-playback",	'1',	O(playback) },
	{ "",			0,		dev_list_prepare },
};
#undef O

static void cmd_dev_list_free(struct cmd_dev_list *l)
{
	ffmem_free(l);
}

static struct ffarg_ctx cmd_dev_list_init()
{
	return SUBCMD_INIT(ffmem_new(struct cmd_dev_list), cmd_dev_list_free, dev_list_action, cmd_dev_list);
}

static const struct ffarg cmd_dev[] = {
	{ "-help",	'1',	dev_help },
	{ "list",	'{',	cmd_dev_list_init },
	{ "",		0,		dev_help },
};
