/** phiola: executor: 'tui' command
2026, Simon Zolin */

static int tui_help()
{
	help_info_write("\
Start ncurses TUI:\n\
    `phiola tui` [INPUT...]\n\
\n\
INPUT                   File name, directory or URL\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_tui {
	ffvec input; // ffstr[]
};

static int tui_input(struct cmd_tui *t, ffstr s)
{
	return cmd_input(&t->input, s);
}

static int tui_action(struct cmd_tui *t)
{
	struct phi_queue_conf qc = {
		.first_filter = &phi_guard_gui,
		.ui_module = "tui2.play",
	};
	x->queue->create(&qc);

	ffstr *it;
	FFSLICE_WALK(&t->input, it) {
		struct phi_queue_entry qe = {
			.url = it->ptr,
		};
		x->queue->add(NULL, &qe);
	}
	ffvec_free(&t->input);

	x->core->mod("tui2.master");
	x->exit_code = 0;
	return 0;
}

static int tui_open()
{
	return 0;
}

#define O(m)  (void*)FF_OFF(struct cmd_tui, m)
static const struct ffarg cmd_tui[] = {
	{ "-help",		0,		tui_help },
	{ "\0\1",		'S',	tui_input },
	{ "",			0,		tui_open },
};
#undef O

static void cmd_tui_free(struct cmd_tui *t)
{
	ffmem_free(t);
}

static struct ffarg_ctx cmd_tui_init(void *obj)
{
	return SUBCMD_INIT(ffmem_new(struct cmd_tui), cmd_tui_free, tui_action, cmd_tui);
}
