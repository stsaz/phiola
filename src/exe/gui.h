/** phiola: executor: 'gui' command
2023, Simon Zolin */

static int gui_help()
{
	help_info_write("\
Show graphical interface:\n\
    `phiola gui` [INPUT...]\n\
\n\
INPUT                   File name, directory or URL\n\
");
	x->exit_code = 0;
	return 1;
}

struct cmd_gui {
	ffvec input; // ffstr[]
};

static int gui_input(struct cmd_gui *g, ffstr s)
{
	return cmd_input(&g->input, s);
}

static void gui_log_ctl(uint flags)
{
	if (flags)
		x->log.func = x->uif->log; // GUI is ready to display logs
	else
		x->log.func = NULL;
}

static int gui_action(struct cmd_gui *g)
{
	const phi_remote_cl_if *rcl = x->core->mod("remote.client");
	ffvec inz = {};
	ffstr *it;
	FFSLICE_WALK(&g->input, it) {
		*ffvec_pushT(&inz, char*) = it->ptr;
	}
	if (!rcl->play("gui", *(ffslice*)&inz, PHI_RCLF_NOLOG)) {
		x->core->sig(PHI_CORE_STOP);
		goto end;
	}

	const phi_remote_sv_if *rsv = x->core->mod("remote.server");
	rsv->start("gui");

	struct phi_queue_conf qc = {
		.first_filter = &phi_guard_gui,
		.ui_module = "gui.track",
	};
	x->queue->create(&qc);

	FFSLICE_WALK(&g->input, it) {
		struct phi_queue_entry qe = {
			.url = it->ptr,
		};
		x->queue->add(NULL, &qe);
	}
	ffvec_free(&g->input);

	x->log.use_color = (x->log.use_color && x->debug);
	x->core->mod("gui");

	if (!x->debug) {
		// show logs inside Logs window
		x->uif = x->core->mod("gui.if");
		struct phi_ui_conf uc = {
			.log_ctl = gui_log_ctl,
		};
		x->uif->conf(&uc);
	}

end:
	ffvec_free(&inz);
	x->exit_code = 0;
	return 0;
}

static int gui_open()
{
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
	return SUBCMD_INIT(ffmem_new(struct cmd_gui), cmd_gui_free, gui_action, cmd_gui);
}
