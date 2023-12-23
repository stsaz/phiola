/** phiola: executor: 'gui' command
2023, Simon Zolin */

static int gui_help()
{
	static const char s[] = "\
Show graphical interface:\n\
    phiola gui [INPUT...]\n\
\n\
INPUT                   File name, directory or URL\n\
";
	ffstdout_write(s, FFS_LEN(s));
	x->exit_code = 0;
	return 1;
}

struct cmd_gui {
	ffvec input; // ffstr[]
};

static int gui_input(struct cmd_gui *g, ffstr s)
{
	*ffvec_pushT(&g->input, ffstr) = s;
	return 0;
}

static int gui_action(struct cmd_gui *g)
{
	struct phi_queue_conf qc = {
		.first_filter = &phi_guard_gui,
		.ui_module = "gui.track",
	};
	x->queue->create(&qc);

	ffstr *it;
	FFSLICE_WALK(&g->input, it) {
		struct phi_queue_entry qe = {
			.conf.ifile.name = ffsz_dupstr(it),
		};
		if (0 == x->queue->add(NULL, &qe))
			x->queue->play(NULL, x->queue->at(NULL, 0));
	}
	ffvec_free(&g->input);

	x->core->mod("gui");

	if (!x->debug) {
		// show logs inside Logs window
		x->log.use_color = 0;
		x->log.func = x->core->mod("gui.log");
	}

	return 0;
}

static int gui_open()
{
	x->action = (int(*)(void*))gui_action;
	return 0;
}

static const struct ffarg cmd_gui[] = {
	{ "-help",		0,		gui_help },
	{ "\0\1",		'S',	gui_input },
	{ "",			0,		gui_open },
};

static void cmd_gui_free(struct cmd_gui *g)
{
	ffmem_free(g);
}

static struct ffarg_ctx cmd_gui_init(void *obj)
{
	x->cmd_data = ffmem_new(struct cmd_gui);
	x->cmd_free = (void(*)(void*))cmd_gui_free;
	struct ffarg_ctx cx = {
		cmd_gui, x->cmd_data
	};
	return cx;
}
