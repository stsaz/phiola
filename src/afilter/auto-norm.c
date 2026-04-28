/** phiola: auto loudness normalizer
2024, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <ffaudio/pcm-gain.h>
#include <ffbase/args.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct autonorm_conf {
	int target;
	int max_gain, max_attenuate;
};

#define O(m)  (void*)(size_t)FF_OFF(struct autonorm_conf, m)
static const struct ffarg autonorm_conf_args[] = {
	{ "attenuate",	'd',	O(max_attenuate) },
	{ "gain",		'd',	O(max_gain) },
	{ "target",		'd',	O(target) },
	{}
};
#undef O

struct autonorm {
	struct pcm_af af;
	double gain_db, gain;
	struct autonorm_conf conf;
};

static void* anorm_open(phi_track *t)
{
	struct autonorm *c = phi_track_allocT(t, struct autonorm);
	c->af = *(struct pcm_af*)&t->oaudio.format;
	struct autonorm_conf conf = {
		.target = -14,
		.max_gain = 6,
		.max_attenuate = -6,
	};
	c->conf = conf;
	struct ffargs a = {};
	if (ffargs_process_line(&a, autonorm_conf_args, &c->conf, FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES, t->conf.afilter.auto_normalizer)) {
		errlog(t, "%s", a.error);
		phi_track_free(t, c);
		return PHI_OPEN_ERR;
	}
	return c;
}

static void anorm_close(void *ctx, phi_track *t)
{
	struct autonorm *c = ctx;
	phi_track_free(t, c);
}

static int anorm_process(void *ctx, phi_track *t)
{
	struct autonorm *c = ctx;

	double db = c->conf.target - t->oaudio.loudness;
	if (t->oaudio.loudness_momentary - t->oaudio.loudness > 2) {
		db = c->conf.target - t->oaudio.loudness_momentary;
		if (isinf(t->oaudio.loudness) && db > 0)
			db = 0;
	}

	if (db > c->conf.max_gain)
		db = c->conf.max_gain;
	else if (db < c->conf.max_attenuate)
		db = c->conf.max_attenuate;

	if (db != c->gain_db) {
		c->gain_db = db;
		c->gain = db_gain(db);
		dbglog(t, "gain: %.02FdB %.02F", c->gain_db, c->gain);
	}

	t->data_out = t->data_in;
	if (db != 0)
		pcm_gain(&c->af, c->gain, t->data_out.ptr, t->data_out.ptr, t->data_out.len / pcm_size1(&c->af));

	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

const phi_filter phi_auto_norm = {
	anorm_open, anorm_close, anorm_process,
	"auto-norm"
};
