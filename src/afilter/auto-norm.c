/** phiola: auto loudness normalizer
2024, Simon Zolin */

#include <track.h>
#include <afilter/pcm.h>
#include <afilter/pcm_gain.h>
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
	uint state;
	struct phi_af af;
	double gain_db, gain;
	ffvec buf;
	struct autonorm_conf conf;
};

static void* anorm_open(phi_track *t)
{
	struct autonorm *c = phi_track_allocT(t, struct autonorm);
	c->af = t->oaudio.format;
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
	ffvec_free(&c->buf);
	phi_track_free(t, c);
}

/**
e.g. for Target=-14:
Loudness  LM  Gain
-inf          (hold)
-8            -6
-14           0
-20           +6
< -10     -8  -6
*/
static int anorm_process(void *ctx, phi_track *t)
{
	struct autonorm *c = ctx;
	t->data_out = t->data_in;

	switch (c->state) {
	case 0:
		c->state = 1;
		return PHI_OK;

	case 1:
		ffvec_addstr(&c->buf, &t->data_in);
		t->data_out = *(ffstr*)&c->buf;
		if (isinf(t->oaudio.loudness)) {
			if (t->chain_flags & PHI_FFIRST)
				return PHI_DONE;
			return PHI_MORE;
		}
		c->state = 2;
		break;

	case 2:
		ffvec_free(&c->buf);
		c->state = 3;
	}

	double db = c->conf.target - t->oaudio.loudness;
	if (t->oaudio.loudness_momentary - t->oaudio.loudness > 2)
		db = c->conf.target - t->oaudio.loudness_momentary;
	if (db > c->conf.max_gain)
		db = c->conf.max_gain;
	else if (db < c->conf.max_attenuate)
		db = c->conf.max_attenuate;

	if (db != c->gain_db) {
		c->gain_db = db;
		c->gain = db_gain(db);
		dbglog(t, "gain: %.02FdB %.02F", c->gain_db, c->gain);
	}

	if (db != 0)
		pcm_gain(&c->af, c->gain, t->data_out.ptr, t->data_out.ptr, t->data_out.len / pcm_size1(&c->af));

	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

const phi_filter phi_auto_norm = {
	anorm_open, anorm_close, anorm_process,
	"auto-norm"
};
