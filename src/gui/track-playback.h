/** phiola: GUI: track control filter
2023, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <util/aformat.h>

#define dbglog1(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)
#define errlog1(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)

struct gtrk {
	phi_track *t;
	uint sample_rate;

	int64 seek_msec;
	uint last_pos_sec;
	uint duration_sec;

	uint opened :1;
	uint have_fmt :1;
	uint paused :1;
};

/** Set volume */
void gtrk_vol_apply(struct gtrk *gt, double db)
{
	gt->t->oaudio.gain_db = db;
}

/** Set/reset 'pause' flag */
void gtrk_play_pause(struct gtrk *gt)
{
	if (gt->paused) {
		gt->paused = 0;
		uint p = gt->t->oaudio.pause;
		gt->t->oaudio.pause = 0;
		dbglog1(gt->t, "unpausing");
		wmain_status_id(ST_UNPAUSED);
		if (!p)
			core->track->wake(gt->t);
		return;
	}

	gt->t->oaudio.pause = 1;
	gt->paused = 1;
	dbglog1(gt->t, "pausing");

	if (gt->t->oaudio.adev_ctx)
		gt->t->oaudio.adev_stop(gt->t->oaudio.adev_ctx);

	wmain_status_id(ST_PAUSED);
}

/** Set seek position */
void gtrk_seek(struct gtrk *gt, uint pos_sec)
{
	gt->seek_msec = pos_sec * 1000;
	gt->t->audio.seek_req = 1;
	gt->t->oaudio.clear = 1;

	if (gt->t->oaudio.adev_ctx)
		gt->t->oaudio.adev_stop(gt->t->oaudio.adev_ctx);

	dbglog1(gt->t, "seek: %U", gt->seek_msec);
}

static void* gtrk_open(phi_track *t)
{
	if (t->qent == NULL) return PHI_OPEN_SKIP;

	struct gtrk *gt = phi_track_allocT(t, struct gtrk);
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
		gd->qe_active = NULL;
		gd->playing_track = NULL;
		if (wmain_ready()) {
			struct gui_track_info *ti = &gd->playback_track_info;
			ti->index_new = gd->queue->index(t->qent);
			gui_task_ptr(wmain_track_close, ti);
		}
	}
	phi_track_free(t, gt);
}

static int handle_seek(struct gtrk *gt, phi_track *t)
{
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
	return 0;
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
		gt->duration_sec = samples_to_msec(t->audio.total, gt->sample_rate) / 1000;
	}

	if ((!gt->opened || t->meta_changed) && wmain_ready()) {

		struct phi_queue_entry *qe = (struct phi_queue_entry*)t->qent;
		char buf[1000];
		ffsz_format(buf, sizeof(buf), "%u kbps, %s, %u Hz, %s, %s"
			, (t->audio.bitrate + 500) / 1000
			, t->audio.decoder
			, t->audio.format.rate
			, phi_af_name(t->audio.format.format)
			, pcm_channelstr(t->audio.format.channels));
		core->metaif->set(&t->meta, FFSTR_Z("_phi_info"), FFSTR_Z(buf), 0);

		qe->length_sec = gt->duration_sec;

		void *qe_prev_active = gd->qe_active;
		gd->qe_active = qe;

		struct gui_track_info *ti = &gd->playback_track_info;
		ti->duration_sec = gt->duration_sec;
		ti->pos_sec = 0;
		ti->index_old = ~0U;
		ti->index_new = ~0U;

		int i;
		if (qe_prev_active
			&& -1 != (i = gd->queue->index(qe_prev_active))
			&& !gd->q_filtered)
			ti->index_old = i;

		if (-1 != (i = gd->queue->index(qe))) {
			if (!gd->q_filtered) // 'i' is the position within the original list, not filtered list
				ti->index_new = i;
			gd->cursor = i;
		}

		ffstr artist = {}, title = {};
		core->metaif->find(&t->meta, FFSTR_Z("artist"), &artist, 0);
		core->metaif->find(&t->meta, FFSTR_Z("title"), &title, 0);
		if (!title.len)
			ffpath_split3_str(FFSTR_Z(t->conf.ifile.name), NULL, &title, NULL); // use filename as a title
		ffsz_format(ti->buf, sizeof(ti->buf), "%S - %S - phiola"
			, &artist, &title);

		// We need to display the currently active track's meta data before `queue` does this on track close
		if (!qe->meta_priority)
			qe_meta_update(qe, &t->meta, core->metaif);

		gui_task_ptr(wmain_track_new, ti);

		gt->opened = 1;
		t->meta_changed = 0;

		if (gd->conf.auto_skip_sec_percent > 0)
			gt->seek_msec = gd->conf.auto_skip_sec_percent * 1000;
		else if (gd->conf.auto_skip_sec_percent < 0)
			gt->seek_msec = gt->duration_sec * -gd->conf.auto_skip_sec_percent / 100 * 1000;
	}

	if (handle_seek(gt, t))
		return PHI_MORE;

	if (t->audio.pos == ~0ULL)
		goto end;

	uint pos_sec = (uint)(samples_to_msec(t->audio.pos, gt->sample_rate) / 1000);
	if (pos_sec == gt->last_pos_sec && !(t->chain_flags & PHI_FFIRST))
		goto end;
	gt->last_pos_sec = pos_sec;

	if (gt == gd->playing_track && wmain_ready()) {
		struct gui_track_info *ti = (struct gui_track_info*)&gd->playback_track_info;
		ti->pos_sec = pos_sec;
		gui_task_ptr(wmain_track_update, ti);
	}

end:
	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_DATA;
}

const phi_filter phi_gui_track = {
	gtrk_open, gtrk_close, gtrk_process,
	"gui"
};
