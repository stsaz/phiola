/** phiola: Windows-subsystem GUI executor
2023, Simon Zolin */

#include <phiola.h>
#include <track.h>
#include <ffsys/process.h>
#include <ffsys/path.h>
#include <ffsys/environ.h>
#include <ffsys/globals.h>
#include <ffbase/vector.h>
#include <ffbase/args.h>

struct ctx {
	const phi_core *core;
	const phi_queue_if *queue;

	char fn[4*1024];
	ffstr root_dir;

	ffvec input; // char*[]
};
static struct ctx *x;

static int conf()
{
	const char *p;
	if (!(p = ffps_filename(x->fn, sizeof(x->fn), NULL)))
		return -1;
	if (ffpath_splitpath_str(FFSTR_Z(p), &x->root_dir, NULL) < 0)
		return -1;
	x->root_dir.len++;
	return 0;
}

static char* env_expand(const char *s)
{
	return ffenv_expand(NULL, NULL, 0, s);
}

static int core()
{
	struct phi_core_conf conf = {
		.env_expand = env_expand,
		.root = x->root_dir,
	};
	if (!(x->core = phi_core_create(&conf)))
		return -1;
	x->queue = x->core->mod("core.queue");
	return 0;
}

static int input(struct ctx *x, char *s)
{
	*ffvec_pushT(&x->input, char*) = s;
	return 0;
}

static void phi_guigrd_close(void *f, phi_track *t)
{
	x->core->track->stop(t);
}

static int phi_grd_process(void *f, phi_track *t)
{
	x->core->track->filter(t, x->core->mod("core.win-sleep"), 0);
	return PHI_DONE;
}

static const phi_filter phi_guard_gui = {
	NULL, phi_guigrd_close, phi_grd_process,
	"gui-guard"
};

static int action()
{
	struct phi_queue_conf qc = {
		.first_filter = &phi_guard_gui,
		.ui_module = "gui.track",
	};
	x->queue->create(&qc);

	char **it;
	FFSLICE_WALK(&x->input, it) {
		struct phi_queue_entry qe = {
			.conf.ifile.name = ffsz_dup(*it),
		};
		if (0 == x->queue->add(NULL, &qe))
			x->queue->play(NULL, x->queue->at(NULL, 0));
	}
	ffvec_free(&x->input);

	x->core->mod("gui");
	return 0;
}

static const struct ffarg cmd_args[] = {
	{ "\0\1",		's',	input },
	{ "",			0,		action },
};

static int cmd()
{
	char *cmd_line = ffsz_alloc_wtou(GetCommandLineW());
	int r;
	struct ffargs a = {};
	if ((r = ffargs_process_line(&a, cmd_args, x, FFARGS_O_SKIP_FIRST, cmd_line)) < 0) {
	}
	ffmem_free(cmd_line);
	return r;
}

static void cleanup()
{
	phi_core_destroy();
	// ffmem_free(x);
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	x = ffmem_new(struct ctx);
	if (conf()) goto end;
	if (core()) goto end;
	if (cmd()) goto end;
	phi_core_run();

end:
	cleanup();
	return 0;
}
