/** phiola: GUI: convert track filter
2023, Simon Zolin */

#include <track.h>

struct conv {
	uint sample_rate;
	int last_pos_sec;
	uint time_total;
};

static void* conv_open(phi_track *t)
{
	struct conv *c = phi_track_allocT(t, struct conv);
	c->last_pos_sec = -1;
	c->sample_rate = t->audio.format.rate;
	c->time_total = samples_to_msec(t->audio.total, c->sample_rate) / 1000;
	wmain_conv_track_new(t, c->time_total);
	return c;
}

static void conv_close(void *ctx, phi_track *t)
{
	struct conv *c = ctx;
	wmain_conv_track_close(t);
	phi_track_free(t, c);
}

static int conv_process(void *ctx, phi_track *t)
{
	struct conv *c = ctx;

	if (t->audio.seek != -1 && !t->audio.seek_req) {
		dbglog1(t, "seek: done");
		t->audio.seek = ~0ULL; // prev. seek is complete
	}

	if (t->audio.pos == ~0ULL)
		goto end;

	uint pos_sec = (uint)(samples_to_msec(t->audio.pos, c->sample_rate) / 1000);
	if (c->last_pos_sec >= 0 && (int)pos_sec < c->last_pos_sec + 10
		&& !(t->chain_flags & PHI_FFIRST))
		goto end;
	c->last_pos_sec = pos_sec;

	wmain_conv_track_update(t, pos_sec, c->time_total);

end:
	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

const phi_filter phi_gui_conv = {
	conv_open, conv_close, conv_process,
	"gui-convert"
};
