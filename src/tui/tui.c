/** phiola: Terminal UI.
2015, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <util/aformat.h>
#include <afilter/pcm.h>
#include <ffsys/std.h>
#include <ffsys/pipe.h>
#include <ffsys/globals.h>

static const phi_core *core;
#define syserrlog(t, ...)  phi_syserrlog(core, "tui", t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, "tui", t, __VA_ARGS__)
#define syswarnlog(t, ...)  phi_syswarnlog(core, "tui", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "tui", t, __VA_ARGS__)
#define userlog(t, ...)  phi_userlog(core, "tui", t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, "tui", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "tui", t, __VA_ARGS__)

typedef struct tui_track {
	phi_track *t;
	uint64 total_samples;
	uint64 played_samples;
	int64 seek_msec;
	uint lastpos;
	uint sample_rate;
	uint sampsize;
	uint total_time_sec;
	ffvec buf;
	uint nback;
	uint show_info :1;
	uint meta_change_seen :1;
	uint paused :1;
} tui_track;

struct tui_rec {
	phi_track *t;
	uint sample_rate;
	uint sampsize;
	uint lastpos;
	uint64 processed_samples;
	ffvec buf;
	double maxdb;
	uint nback;
	uint paused :1;
};

struct tui_mod {
	const phi_queue_if *queue;
	phi_kevent *kev;
	struct phi_woeh_task task_read;

	struct tui_track *curtrk; // currently playing track
	struct tui_rec *curtrk_rec; // currently recording track
	phi_task task_init;

	uint vol;
	uint progress_dots;

	uint activating_track_n;
	phi_timer activating_track_n_tmr;

	struct {
		const char *progress,
			*filename,
			*index,
			*reset;
	} color;

	uint use_stderr :1;
	uint mute :1;
	uint conversion :1;
	uint conversion_valid :1;
	uint term_size_change_subscribed :1;
	uint term_size_changed :1;
};

static struct tui_mod *mod;

enum {
	SEEK_STEP = 5 * 1000,
	SEEK_STEP_MED = 15 * 1000,
	SEEK_STEP_LARGE = 60 * 1000,
	REC_STATUS_UPDATE = 200, //update recording status timeout (msec)

	VOL_STEP = 5,
	VOL_MAX = 125,
	VOL_LO = -40,
	VOL_HI = 6,

	MINDB = 40,
};

enum CMDS {
	CMD_STOP,
	CMD_SEEKRIGHT,
	CMD_SEEKLEFT,
	CMD_VOLUP,
	CMD_VOLDOWN,
	CMD_MUTE,
	CMD_NEXT,
	CMD_PREV,
	CMD_FIRST,
	CMD_LAST,
	CMD_ACTIVATE_N,

	CMD_SHOWTAGS,

	CMD_QUIT,

	CMD_MASK = 0xff,

	_CMD_PLAYBACK = 1 << 26, // Call handler only if we are in playback mode
	_CMD_F3 = 1 << 27, //use 'cmdfunc3'
	_CMD_CURTRK = 1 << 28, // use 'cmdfunc'.  Call handler only if there's an active track
	_CMD_CORE = 1 << 29,
	_CMD_F1 = 1 << 31, //use 'cmdfunc1'. Can't be used with _CMD_CURTRK.
};

typedef void (*cmdfunc)(tui_track *u, uint cmd);
typedef void (*cmdfunc3)(tui_track *u, uint cmd, void *udata);
typedef void (*cmdfunc1)(uint cmd);


static void tui_prepare(uint f);
static void tui_cmd_read(void *param);
static void tui_op(uint cmd);
static void cmd_activate(uint cmd);

static void tui_print(const void *d, ffsize n)
{
	if (mod->use_stderr) {
		ffstderr_write(d, n);
		return;
	}
	ffstdout_write(d, n);
}

#include <tui/play.h>
#include <tui/rec.h>

static void cmd_play()
{
	if (mod->curtrk == NULL) {
		if (mod->curtrk_rec != NULL) {
			tuirec_pause_resume(mod->curtrk_rec);
			return;
		}

		mod->queue->play(NULL, NULL);
		return;
	}

	tuiplay_pause_resume(mod->curtrk);
}

static void cmd_activate(uint cmd)
{
	uint i = 0;
	switch (cmd) {
	case CMD_NEXT:
		mod->queue->play(NULL, PHI_Q_PLAY_NEXT);  return;

	case CMD_PREV:
		mod->queue->play(NULL, PHI_Q_PLAY_PREVIOUS);  return;

	case CMD_FIRST:
		break;

	case CMD_LAST:
		i = mod->queue->count(NULL) - 1;
		break;

	case CMD_ACTIVATE_N:
		i = mod->activating_track_n - 1;
		break;
	}

	void *qe = mod->queue->at(NULL, i);
	if (qe)
		mod->queue->play(NULL, qe);
}

static void cmd_random()
{
	struct phi_queue_conf *qc = mod->queue->conf(NULL);
	qc->random = !qc->random;
	infolog(NULL, "Random: %u", (int)qc->random);
}

static void tui_op(uint cmd)
{
	switch (cmd) {

	case CMD_STOP:
		core->track->cmd(NULL, PHI_TRACK_STOP_ALL);
		break;

	case CMD_QUIT:
		core->sig(PHI_CORE_STOP);
		break;
	}
}

static void list_save()
{
	char *fn = NULL, *tmpdir = "/tmp";
#ifdef FF_WIN
	tmpdir = core->conf.env_expand("%TMP%");
#endif
	fftime t;
	fftime_now(&t);
	fn = ffsz_allocfmt("%s/phiola-%U.m3u8", tmpdir, t.sec);

	mod->queue->save(NULL, fn, NULL, NULL);
	infolog(NULL, "Saved playlist to %s", fn);

	ffmem_free(fn);
#ifdef FF_WIN
	ffmem_free(tmpdir);
#endif
}

static void help_info_write(ffstr s)
{
	ffstr l, k;
	ffvec v = {};

	while (s.len) {
		ffstr_splitby(&s, '`', &l, &s);
		ffstr_splitby(&s, '`', &k, &s);
		ffvec_addfmt(&v, "%S%s%S%s"
			, &l, mod->color.index, &k, mod->color.reset);
	}

	tui_print(v.ptr, v.len);
	ffvec_free(&v);
}

static void tui_help(uint cmd)
{
	ffvec buf = {};
	char *fn = ffsz_allocfmt("%S/mod/tui-help.txt", &core->conf.root);
	if (fffile_readwhole(fn, &buf, 64*1024))
		goto end;
	help_info_write(*(ffstr*)&buf);
end:
	ffvec_free(&buf);
	ffmem_free(fn);
}

struct key {
	uint key;
	uint cmd;
	void *func; // cmdfunc | cmdfunc1
};

static const struct key hotkeys[] = {
	{ ' ', _CMD_PLAYBACK | _CMD_F1 | _CMD_CORE,		cmd_play },
	{ 'L', _CMD_PLAYBACK | _CMD_F1 | _CMD_CORE,		list_save },

	{ 'd', _CMD_CURTRK | _CMD_CORE,					tuiplay_rm },
	{ 'h', _CMD_F1,									tui_help },
	{ 'i', CMD_SHOWTAGS | _CMD_CURTRK | _CMD_CORE,	tui_op_trk },
	{ 'm', CMD_MUTE | _CMD_CURTRK | _CMD_CORE,		tuiplay_vol },
	{ 'n', CMD_NEXT | _CMD_PLAYBACK | _CMD_F1 | _CMD_CORE,		cmd_activate },
	{ 'p', CMD_PREV | _CMD_PLAYBACK | _CMD_F1 | _CMD_CORE,		cmd_activate },
	{ 'q', CMD_QUIT | _CMD_F1 | _CMD_CORE,			tui_op },
	{ 'r', _CMD_PLAYBACK | _CMD_F1 | _CMD_CORE,		cmd_random },
	{ 's', CMD_STOP | _CMD_F1 | _CMD_CORE,			tui_op },
	{ 'x', _CMD_CURTRK | _CMD_CORE,					tuiplay_rm_playnext },

	{ FFKEY_UP,		CMD_VOLUP | _CMD_CURTRK | _CMD_CORE,				tuiplay_vol },
	{ FFKEY_DOWN,	CMD_VOLDOWN | _CMD_CURTRK | _CMD_CORE,				tuiplay_vol },
	{ FFKEY_RIGHT,	CMD_SEEKRIGHT | _CMD_CURTRK | _CMD_F3 | _CMD_CORE,	tuiplay_seek },
	{ FFKEY_LEFT,	CMD_SEEKLEFT | _CMD_CURTRK | _CMD_F3 | _CMD_CORE,	tuiplay_seek },
	{ FFKEY_HOME,	CMD_FIRST | _CMD_PLAYBACK | _CMD_F1 | _CMD_CORE,	cmd_activate },
	{ FFKEY_END,	CMD_LAST | _CMD_PLAYBACK | _CMD_F1 | _CMD_CORE,		cmd_activate },
};

static const struct key* key2cmd(int key)
{
	ffsize i, start = 0;
	uint k = (key & ~FFKEY_MODMASK), n = FF_COUNT(hotkeys);
	while (start != n) {
		i = start + (n - start) / 2;
		if (k == hotkeys[i].key) {
			return &hotkeys[i];
		} else if (k < hotkeys[i].key)
			n = i;
		else
			start = i + 1;
	}
	return NULL;
}

struct corecmd {
	phi_task tsk;
	const struct key *k;
	void *udata;
};

static void tui_corecmd(void *param)
{
	struct corecmd *c = param;
	uint cmd = c->k->cmd;

	if ((cmd & _CMD_PLAYBACK)
		&& (!mod->conversion_valid || mod->conversion))
		goto done;

	if (cmd & _CMD_F1) {
		cmdfunc1 func1 = (void*)c->k->func;
		func1(cmd & CMD_MASK);

	} else if (cmd & _CMD_CURTRK) {
		if (mod->curtrk == NULL)
			goto done;

		if (cmd & _CMD_F3) {
			cmdfunc3 func3 = (void*)c->k->func;
			func3(mod->curtrk, cmd & CMD_MASK, c->udata);

		} else {
			cmdfunc func = (void*)c->k->func;
			func(mod->curtrk, cmd & CMD_MASK);
		}
	}

done:
	ffmem_free(c);
}

static void tui_corecmd_add(const struct key *k, void *udata)
{
	struct corecmd *c = ffmem_new(struct corecmd);
	c->udata = udata;
	c->k = k;
	core->task(0, &c->tsk, tui_corecmd, c);
}

static void tui_stdin_prepare(void *param)
{
	if (core->conf.stdin_busy) return;

	uint kq_attach = 1;

#ifdef FF_WIN
	DWORD mode;
	if (!GetConsoleMode(ffstdin, &mode)) {
		infolog(NULL, "TUI commands won't work because stdin is not a console");
		return; // Asynchronous reading from a pipe is not supported
	}
	kq_attach = 0;

#else
	uint attr = FFSTD_LINEINPUT;
	ffstd_attr(ffstdin, attr, 0);
#endif

	if (kq_attach) {
		mod->kev = core->kev_alloc(0);
		mod->kev->rhandler = tui_cmd_read;
		mod->kev->obj = mod;
		mod->kev->rtask.active = 1;
		if (core->kq_attach(0, mod->kev, ffstdin, 1))
			return;
		if (ffpipe_nonblock(ffstdin, 1))
			syswarnlog(NULL, "ffpipe_nonblock()");
	}

	tui_cmd_read(mod);
}

static void act_trk_tmr(void *param)
{
	cmd_activate(CMD_ACTIVATE_N);
	mod->activating_track_n = 0;
}

static int act_trk_key_process(uint k)
{
	if (k >= '0' && k <= '9') {
		mod->activating_track_n = (mod->activating_track_n * 10) + (k - '0');
		core->timer(0, &mod->activating_track_n_tmr, -800, act_trk_tmr, NULL);
		return 0;
	}
	return -1;
}

static void tui_cmd_read(void *param)
{
	ffstd_ev ev = {};
	ffstr data = {};

	for (;;) {
		if (data.len == 0) {
			int r = ffstd_keyread(ffstdin, &ev, &data);
			if (r == 0)
				break;
			else if (r < 0)
				break;
		}

		ffstr keydata = data;
		uint key = ffstd_keyparse(&data);
		if (key == (uint)-1) {
			data.len = 0;
			continue;
		}

		const struct key *k = key2cmd(key);
		if (k == NULL) {
			if (!act_trk_key_process(key & ~FFKEY_MODMASK))
				continue;
			dbglog(NULL, "unknown key seq %*xb"
				, (ffsize)keydata.len, keydata.ptr);
			continue;
		}

		dbglog(NULL, "received command %u", k->cmd & CMD_MASK);

		if (k->cmd & _CMD_CORE) {
			void *udata = NULL;
			if (k->cmd & _CMD_F3)
				udata = (void*)(ffsize)key;
			else if (key & FFKEY_MODMASK)
				continue;
			tui_corecmd_add(k, udata);

		} else if (k->cmd & _CMD_F1) {
			cmdfunc1 func1 = (void*)k->func;
			func1(k->cmd & ~_CMD_F1);
		}
	}

#ifdef FF_WIN
	if (core->woeh(0, ffstdin, &mod->task_read, tui_cmd_read, NULL, 1)) {
		syswarnlog(NULL, "establishing stdin event receiver");
	}
#endif
}


#define CLR_PROGRESS  FFSTD_CLR(FFSTD_GREEN)
#define CLR_FILENAME  FFSTD_CLR_I(FFSTD_BLUE)
#define CLR_INDEX  FFSTD_CLR(FFSTD_YELLOW)

static void color_init(struct tui_mod *c)
{
	if (c->color.progress) return;

	c->color.progress = "";
	c->color.filename = "";
	c->color.index = "";
	c->color.reset = "";
	fffd fd = (core->conf.stdout_busy) ? ffstderr : ffstdout;
	uint color = !ffstd_attr(fd, FFSTD_VTERM, FFSTD_VTERM);
	if (color) {
		c->color.progress = CLR_PROGRESS;
		c->color.filename = CLR_FILENAME;
		c->color.index = CLR_INDEX;
		c->color.reset = FFSTD_CLR_RESET;
	}
}

#ifdef FF_WIN

static uint term_size()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(ffstdout, &csbi))
		return 80;
    return csbi.dwSize.X;
}

#else

static uint term_size()
{
	struct winsize w;
	if (ioctl(ffstdout, TIOCGWINSZ, &w))
		return 80;
	return w.ws_col;
}

static void term_size_changed(int sig)
{
	mod->term_size_changed = 1;
}

#endif

static void tui_prepare(uint f)
{
	if (f == 0) return;

	if (mod->term_size_changed) {
		mod->term_size_changed = 0;

#ifndef FF_WIN
		if (!mod->term_size_change_subscribed) {
			mod->term_size_change_subscribed = 1;
			signal(SIGWINCH, term_size_changed);
		}
#endif

		int n = term_size();
		dbglog(NULL, "terminal width:%u", n);
		n -= FFS_LEN("[] 000:00 / 000:00");
		mod->progress_dots = ffmax(n, 0);
	}

	color_init(mod);
}

static void tui_create()
{
	mod->vol = 100;
	mod->queue = core->mod("core.queue");
	mod->use_stderr = core->conf.stdout_busy;
	mod->term_size_changed = 1;
	core->task(0, &mod->task_init, tui_stdin_prepare, NULL);
}

static void tui_destroy()
{
	if (mod == NULL) return;

	uint attr = FFSTD_LINEINPUT;
	ffstd_attr(ffstdin, attr, attr);

	core->timer(0, &mod->activating_track_n_tmr, 0, NULL, NULL);
	ffmem_free(mod);
}

static void tui_conf(struct phi_ui_conf *c)
{
	if (c->volume_percent != 0xff) {
		mod->vol = ffmin((uint)c->volume_percent, VOL_MAX);
		if (mod->curtrk)
			tuiplay_vol(mod->curtrk, ~0U);
	}
}

static void tui_play_seek(uint val, uint flags)
{
	if (!mod->curtrk) return;

	uint cmd = CMD_SEEKRIGHT;
	if (flags & PHI_UI_SEEK_BACK) cmd = CMD_SEEKLEFT;
	tuiplay_seek(mod->curtrk, cmd, (void*)0);
}

static struct phi_ui_if phi_tui_if = {
	.conf = tui_conf,
	.seek = tui_play_seek,
};

static const void* tui_iface(const char *name)
{
	static const struct map_sz_vptr ifs[] = {
		{"if",		&phi_tui_if},
		{"play",	&phi_tuiplay},
		{"rec",		&phi_tuirec},
	};
	return map_sz_vptr_findz2(ifs, FF_COUNT(ifs), name);
}

static const phi_mod phi_tui_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	tui_iface, tui_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_new(struct tui_mod);
	tui_create();
	return &phi_tui_mod;
}
