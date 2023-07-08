/** phiola: audio gain
2022, Simon Zolin */

#include <track.h>
#include <afilter/pcm_gain.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct gain {
	struct phi_af pcm;
	uint samp_size;
	int db;
	double gain;
};

static void* gain_open(phi_track *t)
{
	struct gain *c = ffmem_new(struct gain);
	c->pcm = t->audio.format;
	c->samp_size = pcm_size1(&c->pcm);
	t->audio.gain_db = (t->audio.gain_db) ? t->audio.gain_db : t->conf.afilter.gain_db;
	c->db = -t->audio.gain_db;
	return c;
}

static void gain_close(void *ctx, phi_track *t)
{
	struct gain *c = ctx;
	ffmem_free(c);
}

static int gain_process(void *ctx, phi_track *t)
{
	struct gain *c = ctx;
	double db = t->audio.gain_db;
	if (db != 0) {
		if (db != c->db) {
			c->db = db;
			c->gain = db_gain(db);
			dbglog(t, "gain: %.02FdB %.02F", db, c->gain);
		}
		pcm_gain(&c->pcm, c->gain, t->data_in.ptr, (void*)t->data_in.ptr, t->data_in.len / c->samp_size);
	}

	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

const phi_filter phi_gain = {
	gain_open, gain_close, gain_process,
	"gain"
};
