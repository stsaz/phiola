/** phiola: audio gain
2022, Simon Zolin */

#include <track.h>
#include <afilter/pcm_gain.h>

extern const phi_core *core;
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct gain {
	struct phi_af af;
	uint sample_size;
	double db, gain;
};

static void* gain_open(phi_track *t)
{
	struct gain *c = phi_track_allocT(t, struct gain);
	c->af = (t->oaudio.format.format) ? t->oaudio.format : t->audio.format;
	c->sample_size = pcm_size1(&c->af);
	t->oaudio.gain_db = (t->oaudio.gain_db) ? t->oaudio.gain_db : t->conf.afilter.gain_db;
	c->db = -t->oaudio.gain_db;
	return c;
}

static void gain_close(void *ctx, phi_track *t)
{
	struct gain *c = ctx;
	phi_track_free(t, c);
}

static int gain_process(void *ctx, phi_track *t)
{
	struct gain *c = ctx;
	double db = t->oaudio.replay_gain_db + t->oaudio.gain_db;
	if (db != 0) {
		if (db != c->db) {
			c->db = db;
			c->gain = db_gain(db);
			dbglog(t, "gain: %.02FdB %.02F", db, c->gain);
		}
		pcm_gain(&c->af, c->gain, t->data_in.ptr, (void*)t->data_in.ptr, t->data_in.len / c->sample_size);
	}

	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

const phi_filter phi_gain = {
	gain_open, gain_close, gain_process,
	"gain"
};
