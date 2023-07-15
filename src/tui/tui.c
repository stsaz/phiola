/** phiola: Terminal UI.
2015, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <afilter/pcm.h>
#include <FFOS/std.h>
#include <FFOS/pipe.h>
#include <FFOS/ffos-extern.h>

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
	fftask task_read;

	struct tui_track *curtrk; // currently playing track
	struct tui_rec *curtrk_rec; // currently recording track
	const phi_meta_if *phi_metaif;
	fftask task_init;

	uint vol;
	uint progress_dots;

	struct {
		const char *progress,
			*filename,
			*index,
			*reset;
	} color;

	uint mute :1;
};

static struct tui_mod *mod;

enum {
	SEEK_STEP = 5 * 1000,
	SEEK_STEP_MED = 15 * 1000,
	SEEK_STEP_LARGE = 60 * 1000,
	REC_STATUS_UPDATE = 200, //update recording status timeout (msec)

	VOL_STEP = 5,
	VOL_MAX = 125,
	VOL_LO = /*-*/48,
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

	CMD_SHOWTAGS,

	CMD_QUIT,

	CMD_MASK = 0xff,

	_CMD_CURTRK_REC = 1 << 26,
	_CMD_F3 = 1 << 27, //use 'cmdfunc3'
	_CMD_CURTRK = 1 << 28, // use 'cmdfunc'.  Call handler only if there's an active track
	_CMD_CORE = 1 << 29,
	_CMD_F1 = 1 << 31, //use 'cmdfunc1'. Can't be used with _CMD_CURTRK.
};

typedef void (*cmdfunc)(tui_track *u, uint cmd);
typedef void (*cmdfunc3)(tui_track *u, uint cmd, void *udata);
typedef void (*cmdfunc1)(uint cmd);


static void tui_cmd_read(void *param);
static void tui_help(uint cmd);
static void tui_op(uint cmd);
static void cmd_next();

static const char pcm_fmtstr[][8] = {
	"float32",
	"float64",
	"int16",
	"int24",
	"int24-4",
	"int32",
	"int8",
};
static const ushort pcm_fmt[] = {
	PHI_PCM_FLOAT32,
	PHI_PCM_FLOAT64,
	PHI_PCM_16,
	PHI_PCM_24,
	PHI_PCM_24_4,
	PHI_PCM_32,
	PHI_PCM_8,
};

static const char* pcm_fmt_str(uint fmt)
{
	int r = ffarrint16_find(pcm_fmt, FF_COUNT(pcm_fmt), fmt);
	if (r < 0) {
		return "";
	}
	return pcm_fmtstr[r];
}

static const char *const _pcm_channelstr[] = {
	"mono", "stereo",
	"3-channel", "4-channel", "5-channel",
	"5.1", "6.1", "7.1"
};

static const char* pcm_channelstr(uint channels)
{
	return _pcm_channelstr[ffmin(channels - 1, FF_COUNT(_pcm_channelstr) - 1)];
}

#define samples_to_msec(samples, rate)   ((uint64)(samples) * 1000 / (rate))

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

static void cmd_next()
{
	mod->queue->play_next(NULL);
}

static void cmd_prev()
{
	mod->queue->play_previous(NULL);
}

static void cmd_random()
{
	void *qe = (mod->curtrk) ? mod->curtrk->t->qent : NULL;
	struct phi_queue_conf *qc = mod->queue->conf(qe);
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
	fftime t;
	fftime_now(&t);
	fn = ffsz_allocfmt("%s/phiola-%U.m3u8", tmpdir, t.sec);

	mod->queue->save(NULL, fn);
	infolog(NULL, "Saved playlist to %s", fn);

	ffmem_free(fn);
}

struct key {
	uint key;
	uint cmd;
	void *func; // cmdfunc | cmdfunc1
};

static struct key hotkeys[] = {
	{ ' ',	_CMD_F1 | _CMD_CORE,	cmd_play },
	{ 'L',	_CMD_F1 | _CMD_CORE,	list_save },

	{ 'd',	_CMD_CURTRK | _CMD_CORE,	tuiplay_rm },
	{ 'h',	_CMD_F1,	&tui_help },
	{ 'i',	CMD_SHOWTAGS | _CMD_CURTRK | _CMD_CORE,	&tui_op_trk },
	{ 'm',	CMD_MUTE | _CMD_CURTRK | _CMD_CORE,	&tuiplay_vol },
	{ 'n',	_CMD_F1 | _CMD_CORE,	cmd_next },
	{ 'p',	_CMD_F1 | _CMD_CORE,	cmd_prev },
	{ 'q',	CMD_QUIT | _CMD_F1 | _CMD_CORE,	&tui_op },
	{ 'r',	_CMD_F1 | _CMD_CORE,	cmd_random },
	{ 's',	CMD_STOP | _CMD_F1 | _CMD_CORE,	&tui_op },
	{ 'x',	_CMD_CURTRK | _CMD_CORE,	tuiplay_rm_playnext },

	{ FFKEY_UP,	CMD_VOLUP | _CMD_CURTRK | _CMD_CORE,	&tuiplay_vol },
	{ FFKEY_DOWN,	CMD_VOLDOWN | _CMD_CURTRK | _CMD_CORE,	&tuiplay_vol },
	{ FFKEY_RIGHT,	CMD_SEEKRIGHT | _CMD_F3 | _CMD_CORE,	&tuiplay_seek },
	{ FFKEY_LEFT,	CMD_SEEKLEFT | _CMD_F3 | _CMD_CORE,	&tuiplay_seek },
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
	fftask tsk;
	const struct key *k;
	void *udata;
};

static void tui_help(uint cmd)
{
	ffvec buf = {};
	char *fn = ffsz_allocfmt("%S/mod/tui-help.txt", &core->conf.root);
	if (!!fffile_readwhole(fn, &buf, 64*1024))
		goto end;
	userlog(NULL, "%S", &buf);
end:
	ffvec_free(&buf);
	ffmem_free(fn);
}

static void tui_corecmd(void *param)
{
	struct corecmd *c = param;

	if (c->k->cmd & _CMD_F1) {
		cmdfunc1 func1 = (void*)c->k->func;
		func1(c->k->cmd & CMD_MASK);

	} else if (c->k->cmd & _CMD_F3) {
		if (mod->curtrk == NULL)
			goto done;
		cmdfunc3 func3 = (void*)c->k->func;
		func3(mod->curtrk, c->k->cmd & CMD_MASK, c->udata);

	} else if (c->k->cmd & (_CMD_CURTRK | _CMD_CURTRK_REC)) {
		cmdfunc func = (void*)c->k->func;
		struct tui_track *u = NULL;
		if ((c->k->cmd & _CMD_CURTRK) && mod->curtrk != NULL)
			u = mod->curtrk;
		if (u == NULL)
			goto done;
		func(u, c->k->cmd & CMD_MASK);
	}

done:
	ffmem_free(c);
}

static void tui_corecmd_add(const struct key *k, void *udata)
{
	struct corecmd *c = ffmem_new(struct corecmd);
	c->udata = udata;
	c->k = k;
	core->task(&c->tsk, tui_corecmd, c);
}

static void tui_stdin_prepare(void *param)
{
	if (core->conf.stdin_busy) return;

	uint attr = FFSTD_LINEINPUT;
	ffstd_attr(ffstdin, attr, 0);

#ifdef FF_WIN
	if (0 != core->woeh(ffstdin, &mod->task_read, tui_cmd_read, NULL)) {
		warnlog(NULL, "can't start stdin reader");
		return;
	}

#else
	mod->kev = core->kev_alloc();
	mod->kev->rhandler = tui_cmd_read;
	mod->kev->obj = mod;
	mod->kev->rtask.active = 1;
	if (0 != core->kq_attach(mod->kev, ffstdin, 1)) {
		syswarnlog(NULL, "ffkev_attach()");
		return;
	}
	if (!!fffile_nonblock(ffstdin, 1))
		syswarnlog(NULL, "fffile_nonblock()");
#endif

	tui_cmd_read(mod);
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
			dbglog(NULL, "unknown key seq %*xb"
				, (ffsize)keydata.len, keydata.ptr);
			continue;
		}
		dbglog(NULL, "received command %u", k->cmd & CMD_MASK);

		if (k->cmd & _CMD_CORE) {
			void *udata = NULL;
			if (k->cmd & _CMD_F3)
				udata = (void*)(ffsize)key;
			tui_corecmd_add(k, udata);

		} else if (k->cmd & _CMD_F1) {
			cmdfunc1 func1 = (void*)k->func;
			func1(k->cmd & ~_CMD_F1);
		}
	}
}


#define CLR_PROGRESS  FFSTD_CLR(FFSTD_GREEN)
#define CLR_FILENAME  FFSTD_CLR_I(FFSTD_BLUE)
#define CLR_INDEX  FFSTD_CLR(FFSTD_YELLOW)

static void color_init(struct tui_mod *c)
{
	c->color.progress = "";
	c->color.filename = "";
	c->color.index = "";
	c->color.reset = "";
	uint stderr_color = !ffstd_attr(ffstderr, FFSTD_VTERM, FFSTD_VTERM);
	if (stderr_color) {
		c->color.progress = CLR_PROGRESS;
		c->color.filename = CLR_FILENAME;
		c->color.index = CLR_INDEX;
		c->color.reset = FFSTD_CLR_RESET;
	}
}

static int tui_create()
{
	mod = ffmem_new(struct tui_mod);
	mod->vol = 100;
	mod->queue = core->mod("core.queue");
	mod->phi_metaif = core->mod("format.meta");
	color_init(mod);

	uint term_wnd_size = 80;
	mod->progress_dots = term_wnd_size - FFS_LEN("[] 00:00 / 00:00");

	core->task(&mod->task_init, tui_stdin_prepare, NULL);
	return 0;
}

static void tui_destroy()
{
	if (mod == NULL) return;

	uint attr = FFSTD_LINEINPUT;
	ffstd_attr(ffstdin, attr, attr);

	ffmem_free(mod);
}

static const void* tui_iface(const char *name)
{
	if (ffsz_eq(name, "play")) return &phi_tuiplay;
	else if (ffsz_eq(name, "rec")) return &phi_tuirec;
	return NULL;
}

static const phi_mod phi_tui_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	tui_iface, tui_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	if (!!tui_create())
		return NULL;
	return &phi_tui_mod;
}