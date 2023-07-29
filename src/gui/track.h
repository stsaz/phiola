/** phiola: GUI: track control filter
2023, Simon Zolin */

#include <track.h>
#include <util/util.h>

#define dbglog1(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)
#define errlog1(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)

struct gtrk {
	phi_track *t;
	uint sample_rate;

	int64 seek_msec;
	uint last_pos_sec;
	uint time_total;

	uint opened :1;
	uint have_fmt :1;
	uint paused :1;
};

/** Set volume */
void gtrk_vol_apply(struct gtrk *gt, double db)
{
	gt->t->audio.gain_db = db;
}

/** Set/reset 'pause' flag */
void gtrk_play_pause(struct gtrk *gt)
{
	if (gt->paused) {
		gt->paused = 0;
		uint p = gt->t->oaudio.pause;
		gt->t->oaudio.pause = 0;
		dbglog1(gt->t, "unpausing");
		wmain_status("");
		if (!p)
			core->track->wake(gt->t);
		return;
	}

	gt->t->oaudio.pause = 1;
	gt->paused = 1;
	dbglog1(gt->t, "pausing");
	wmain_status("Paused");
}

/** Set seek position */
void gtrk_seek(struct gtrk *gt, uint pos_sec)
{
	gt->seek_msec = pos_sec * 1000;
	gt->t->audio.seek_req = 1;
	dbglog1(gt->t, "seek: %U", gt->seek_msec);
}

static void* gtrk_open(phi_track *t)
{
	if (t->qent == NULL) return PHI_OPEN_SKIP;

	struct gtrk *gt = ffmem_new(struct gtrk);
	gt->t = t;
	gt->last_pos_sec = -1;
	gt->seek_msec = -1;
	gtrk_vol_apply(gt, gd->gain_db);

	gd->playing_track = gt;
	return gt;
}

static void gtrk_close(void *ctx, phi_track *t)
{
	struct gtrk *gt = ctx;
	if (gd->playing_track == gt) {
		gd->playing_track = NULL;
		wmain_track_close();
	}
	ffmem_free(gt);
}

static int gtrk_process(void *ctx, phi_track *t)
{
	struct gtrk *gt = ctx;

	if (!gt->have_fmt) {
		gt->have_fmt = 1;
		if (t->audio.format.format == 0) {
			errlog1(t, "audio format isn't set");
			return PHI_ERR;
		}

		gt->sample_rate = t->audio.format.rate;
		gt->time_total = samples_to_msec(t->audio.total, gt->sample_rate) / 1000;
	}

	if (!gt->opened) {
		if (!wmain_track_new(gt->t->qent, gt->time_total, t))
			gt->opened = 1;
	}

	if (gt->seek_msec != -1) {
		t->audio.seek = gt->seek_msec;
		gt->seek_msec = -1;
		return PHI_MORE; // new seek request

	} else if (!(t->chain_flags & PHI_FFWD)) {
		return PHI_MORE; // going back without seeking

	} else if (t->data_in.len == 0 && !(t->chain_flags & PHI_FFIRST)) {
		return PHI_MORE; // waiting for audio data

	} else if (t->audio.seek != -1 && !t->audio.seek_req) {
		dbglog1(gt->t, "seek: done");
		t->audio.seek = ~0ULL; // prev. seek is complete
	}

	if (t->audio.pos == ~0ULL)
		goto done;

	uint pos_sec = (uint)(samples_to_msec(t->audio.pos, gt->sample_rate) / 1000);
	if (pos_sec == gt->last_pos_sec && !(t->chain_flags & PHI_FFIRST))
		goto done;
	gt->last_pos_sec = pos_sec;

	if (gt == gd->playing_track)
		wmain_track_update(pos_sec, gt->time_total);

done:
	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_DATA;
}

const phi_filter phi_gui_track = {
	gtrk_open, gtrk_close, gtrk_process,
	"gui"
};
