/** phiola: Remote control
2023, Simon Zolin */

#include <track.h>
#include <ffsys/pipe.h>
#include <ffsys/globals.h>
#include <ffbase/args.h>

#define syserrlog(c, ...)  phi_syserrlog(c->core, "remote-ctl", NULL, __VA_ARGS__)
#define errlog(c, ...)  phi_errlog(c->core, "remote-ctl", NULL, __VA_ARGS__)
#define dbglog(c, ...)  phi_dbglog(c->core, "remote-ctl", NULL, __VA_ARGS__)

struct remote_ctl {
	const phi_core*		core;
	const phi_queue_if*	queue;
	const phi_ui_if*	ui;

	phi_kevent	server_kev;
	fffd		server_pipe;
	fffd		client_pipe;
	ffkq_task	accept_task;
	ffvec		pipe_name;
};
static struct remote_ctl *g;

static int cmd_start(void *o, ffstr s)
{
	struct phi_queue_entry qe = {
		.url = ffsz_dupstr(&s),
	};
	int i = g->queue->add(NULL, &qe);
	ffmem_free(qe.url);
	g->queue->play(NULL, g->queue->at(NULL, i));
	return 0;
}

static int cmd_clear() {
	g->queue->clear(NULL);
	return 0;
}

static int cmd_play() {
	g->queue->play(NULL, NULL);
	return 0;
}

static int cmd_next() {
	g->queue->play_next(NULL);
	return 0;
}

static int cmd_previous() {
	g->queue->play_previous(NULL);
	return 0;
}

static int cmd_stop() {
	g->core->track->cmd(NULL, PHI_TRACK_STOP_ALL);
	return 0;
}

static int cmd_quit() {
	g->core->sig(PHI_CORE_STOP);
	return 0;
}

static int cmd_seek(void *o, ffstr s) {
	uint f;
	if (ffstr_eqz(&s, "forward"))
		f = PHI_UI_SEEK_FWD;
	else if (ffstr_eqz(&s, "back"))
		f = PHI_UI_SEEK_BACK;
	else
		return 0;

	if (!g->ui)
		g->ui = g->core->mod("tui.if");
	g->ui->seek(0, f);
	return 0;
}

static int cmd_volume(void *o, uint64 n) {
	struct phi_ui_conf uc = {
		.volume_percent = n,
	};
	if (!g->ui)
		g->ui = g->core->mod("tui.if");
	g->ui->conf(&uc);
	return 0;
}

static const struct ffarg args[] = {
	{ "clear",		'1',	cmd_clear },
	{ "next",		'1',	cmd_next },
	{ "play",		'1',	cmd_play },
	{ "previous",	'1',	cmd_previous },
	{ "quit",		'1',	cmd_quit },
	{ "seek",		'S',	cmd_seek },
	{ "start",		'S',	cmd_start },
	{ "stop",		'1',	cmd_stop },
	{ "volume",		'u',	cmd_volume },
	{}
};

static int rctl_parse(const char *cmd)
{
	if (cmd[0] == '\0') return -1;

	struct ffargs a = {};
	int r = ffargs_process_line(&a, args, g, 0, cmd);
	if (r < 0) {
		errlog(g, "'%s': %s", cmd, a.error);
		return -1;
	}
	return 0;
}

static void rctl_process(fffd peer)
{
	ffvec buf = {};
	ffvec_alloc(&buf, 1024, 1);

	for (;;) {
		ffssize r = ffpipe_read(peer, ffslice_end(&buf, 1), ffvec_unused(&buf));
		if (r < 0) {
			syserrlog(g, "pipe read");
			break;
		} else if (r == 0) {
			break;
		}
		buf.len += r;
		dbglog(g, "read %L bytes", r);
		ffvec_grow(&buf, 1024, 1);
	}
	ffvec_addchar(&buf, '\0');
	rctl_parse(buf.ptr);
	ffvec_free(&buf);
}

static int rctl_accept1()
{
	fffd peer = ffpipe_accept_async(g->server_pipe, &g->accept_task);
	if (peer == FFPIPE_NULL) {
		if (fferr_last() != FFPIPE_EINPROGRESS)
			syserrlog(g, "pipe accept: %s", g->pipe_name.ptr);
		return -1;
	}
	dbglog(g, "accepted client");
	rctl_process(peer);
	ffpipe_peer_close(peer);
	return 0;
}

static void rctl_accept(void *param)
{
	for (;;) {
		if (rctl_accept1())
			break;
	}
}

static int rctl_listen()
{
	if (FFPIPE_NULL == (g->server_pipe = ffpipe_create_named(g->pipe_name.ptr, FFPIPE_ASYNC))) {
		syserrlog(g, "pipe create: %s", g->pipe_name.ptr);
		return -1;
	}
	dbglog(g, "created pipe: %s", g->pipe_name.ptr);

	g->server_kev.rhandler = rctl_accept;
	g->server_kev.obj = g;
	g->server_kev.rtask.active = 1;
	if (g->core->kq_attach(0, &g->server_kev, g->server_pipe, 1)) {
		ffpipe_close(g->server_pipe);
		return -1;
	}

	return 0;
}

static void rctl_prepare(const char *name)
{
	if (!name)
		name = "";
	g->pipe_name.len = 0;
#ifdef FF_UNIX
	uint uid = getuid();
	ffvec_addfmt(&g->pipe_name, "/tmp/phiola%s-%u.sock%Z", name, uid);
#else
	ffvec_addfmt(&g->pipe_name, "\\\\.\\pipe\\phiola%s%Z", name);
#endif
}

static int rctl_start(const char *name)
{
	if (g->server_pipe != FFPIPE_NULL) return -1; // only 1 server instance is supported

	rctl_prepare(name);
	if (rctl_listen())
		return -1;
	g->queue = g->core->mod("core.queue");
	rctl_accept(NULL);
	return 0;
}

static const struct phi_remote_sv_if phi_rctl_sv = {
	rctl_start
};


static int rctl_connect(uint flags)
{
	if (FFPIPE_NULL == (g->client_pipe = ffpipe_connect(g->pipe_name.ptr))) {
		if (!(flags & PHI_RCLF_NOLOG))
			syserrlog(g, "pipe connect: %s", g->pipe_name.ptr);
		return -1;
	}
	dbglog(g, "connected");
	return 0;
}

static int rctl_cmd(const char *name, ffstr cmd)
{
	int rc = -1;
	rctl_prepare(name);
	if (rctl_connect(0))
		return -1;

	if (cmd.len != (ffsize)ffpipe_write(g->client_pipe, cmd.ptr, cmd.len)) {
		syserrlog(g, "pipe write");
		goto end;
	}
	dbglog(g, "written %L bytes", cmd.len);
	rc = 0;

end:
	ffpipe_close(g->client_pipe);  g->client_pipe = FFPIPE_NULL;
	return rc;
}

static int rctl_play(const char *name, ffslice names, uint flags)
{
	int rc = -1;
	rctl_prepare(name);
	if (rctl_connect(flags))
		return -1;

	ffvec cmd = {};
	ffvec_addsz(&cmd, "start ");
	const char **it;
	FFSLICE_WALK(&names, it) {
		ffvec_addfmt(&cmd, "\"%s\" ", *it);
	}

	if (cmd.len != (ffsize)ffpipe_write(g->client_pipe, cmd.ptr, cmd.len)) {
		syserrlog(g, "pipe write");
		goto end;
	}
	dbglog(g, "written %L bytes", cmd.len);
	rc = 0;

end:
	ffvec_free(&cmd);
	ffpipe_close(g->client_pipe);  g->client_pipe = FFPIPE_NULL;
	return rc;
}

static const struct phi_remote_cl_if phi_rctl_cl = {
	rctl_cmd, rctl_play,
};


static void rctl_init(const phi_core *core)
{
	g = ffmem_new(struct remote_ctl);
	g->core = core;
	g->server_pipe = FFPIPE_NULL;
	g->client_pipe = FFPIPE_NULL;
}

static void rctl_destroy()
{
	if (g->server_pipe != FFPIPE_NULL) {
		ffpipe_close(g->server_pipe);
#ifdef FF_UNIX
		fffile_remove(g->pipe_name.ptr);
#endif
	}

	if (g->client_pipe != FFPIPE_NULL)
		ffpipe_close(g->client_pipe);

	ffvec_free(&g->pipe_name);
	ffmem_free(g);  g = NULL;
}

static const void* rctl_iface(const char *name)
{
	if (ffsz_eq(name, "client")) return &phi_rctl_cl;
	if (ffsz_eq(name, "server")) return &phi_rctl_sv;
	return NULL;
}

static const phi_mod phi_mod_remote_ctl = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	rctl_iface, rctl_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *core)
{
	rctl_init(core);
	return &phi_mod_remote_ctl;
}
