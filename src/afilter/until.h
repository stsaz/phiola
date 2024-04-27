/** phiola: '-until' filter
2015,2022, Simon Zolin */

#include <track.h>
#include <afilter/pcm.h>

struct until {
	uint64 until;
	uint64 total;
	uint sampsize;
};

static void* until_open(phi_track *t)
{
	if (t->conf.until_msec == 0)
		return PHI_OPEN_SKIP;

	struct until *u;
	u = phi_track_allocT(t, struct until);
	u->until = msec_to_samples(t->conf.until_msec, t->audio.format.rate);

	u->sampsize = pcm_size(t->audio.format.format, t->audio.format.channels);

	if (t->audio.total != ~0ULL)
		t->audio.total = u->until;
	return u;
}

static void until_close(void *ctx, phi_track *t)
{
	struct until *u = ctx;
	phi_track_free(t, u);
}

static int until_process(void *ctx, phi_track *t)
{
	struct until *u = ctx;
	uint samps;
	uint64 pos;

	t->data_out = t->data_in;

	if (t->chain_flags & PHI_FFIRST)
		return PHI_DONE;

	pos = t->audio.pos;
	if (t->audio.pos == ~0ULL) {
		pos = u->total;
		u->total += t->data_in.len / u->sampsize;
	}

	if (t->conf.stream_copy) {
		if (pos >= u->until) {
			dbglog(t, "reached sample #%U", u->until);

			return PHI_LASTOUT;
		}
		t->data_in.len = 0;
		return PHI_OK;
	}

	samps = t->data_in.len / u->sampsize;
	dbglog(t, "at %U..%U", pos, pos + samps);
	t->data_in.len = 0;
	if (pos + samps >= u->until) {
		dbglog(t, "reached sample #%U", u->until);
		t->data_out.len = (u->until > pos) ? (u->until - pos) * u->sampsize : 0;
		return PHI_LASTOUT;
	}

	return PHI_OK;
}

const phi_filter phi_until = {
	until_open, until_close, until_process,
	"until"
};
