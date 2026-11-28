/** phiola: HTTP remote control server: playback track filter
2026, Simon Zolin */

#include <util/util.h>

struct hc_play {
	phi_track *trk;
	int seek_delta_sec;
	uint seek_abs_sec;
	uint paused :1;
};

struct play_track_status {
	ffstr artist, title;
	uint64 pos_sec, total_sec;
	uint paused :1;
};

static void hc_play_status(struct hc_play *p, struct play_track_status *pts)
{
	phi_track *t = p->trk;
	const struct phi_queue_entry *qe = t->qent;
	core->metaif->find(&t->meta, FFSTR_Z("artist"), &pts->artist, 0);
	core->metaif->find(&t->meta, FFSTR_Z("title"), &pts->title, 0);
	if (!pts->title.len)
		ffpath_split3_str(FFSTR_Z(qe->url), NULL, &pts->title, NULL); // Use file name as title
	pts->pos_sec = (t->audio.pos != ~0ULL) ? samples_to_msec(t->audio.pos, t->audio.format.rate) / 1000 : 0;
	pts->total_sec = (t->audio.total != ~0ULL) ? samples_to_msec(t->audio.total, t->audio.format.rate) / 1000 : 0;
	pts->paused = p->paused;
}

static void hc_play_pause(struct hc_play *p)
{
	if (!p) return;
	uint unpause = p->paused;
	uint adev_pause_handled = !p->trk->oaudio.pause;

	p->paused = !p->paused;
	p->trk->oaudio.pause = p->paused;

	if (unpause) {
		if (adev_pause_handled)
			core->track->wake(p->trk);
	} else {
		if (p->trk->oaudio.adev_ctx)
			p->trk->oaudio.adev_stop(p->trk->oaudio.adev_ctx);
	}

	userlog((unpause) ? "resumed" : "paused");
}

static void hc_play_seek(struct hc_play *p, uint pos_pct)
{
	if (!p) return;
	phi_track *t = p->trk;
	if (t->audio.total == ~0ULL)
		return;
	uint total_sec = (uint)(samples_to_msec(t->audio.total, t->audio.format.rate) / 1000);
	if (!total_sec)
		return;

	p->seek_abs_sec = FFINT_DIVSAFE(total_sec * pos_pct, 100);

	t->audio.seek_req = 1;
	t->oaudio.clear = 1;
	if (t->oaudio.adev_ctx)
		t->oaudio.adev_stop(t->oaudio.adev_ctx);
	core->track->wake(t);
}

static void hc_play_volume(struct hc_play *p)
{
	if (!p) return;
	p->trk->oaudio.gain_db = volume_db;
}

static int hc_play_seek_r(struct hc_play *p)
{
	phi_track *t = p->trk;
	if (p->seek_abs_sec != ~0U) {
		t->audio.seek = p->seek_abs_sec * 1000;
		p->seek_abs_sec = ~0U;
		return PHI_MORE; // new seek request

	} else if (p->seek_delta_sec) {
		t->audio.seek = samples_to_msec(t->audio.pos, t->audio.format.rate) + p->seek_delta_sec * 1000;
		t->audio.seek = ffmax(t->audio.seek, 0);
		p->seek_delta_sec = 0;
		return PHI_MORE; // new seek request

	} else if (!(t->chain_flags & PHI_FFWD)) {
		t->meta_changed = 0;
		return PHI_MORE; // going back without seeking

	} else if (t->data_in.len == 0 && !(t->chain_flags & PHI_FFIRST)) {
		return PHI_MORE; // waiting for audio data

	} else if (t->audio.seek != -1 && !t->audio.seek_req) {
		t->audio.seek = ~0ULL; // prev. seek is complete
	}
	return 0;
}

static void* hc_play_open(phi_track *t)
{
	struct hc_play *p = phi_track_allocT(t, struct hc_play);
	p->trk = t;
	p->seek_abs_sec = ~0U;
	if (volume_db)
		t->oaudio.gain_db = volume_db;
	userlog("playing %s", t->conf.ifile.name);
	hcsv_track_ev(p, 0);
	return p;
}

static void hc_play_close(struct hc_play *p, phi_track *t)
{
	hcsv_track_ev(p, 1);
	phi_track_free(t, p);
}

static int hc_play_process(struct hc_play *p, phi_track *t)
{
	if (t->chain_flags & PHI_FSTOP)
		return PHI_FIN;

	uint play_time = (t->audio.pos != ~0ULL) ? (uint)(samples_to_msec(t->audio.pos, t->audio.format.rate) / 1000) : 0;
	dbglog("%u:%02u"
		, play_time / 60, play_time % 60);

	if (hc_play_seek_r(p))
		return PHI_MORE;

	t->data_out = t->data_in;
	return !(t->chain_flags & PHI_FFIRST) ? PHI_DATA : PHI_DONE;
}

const phi_filter hctl_play_if = {
	hc_play_open, (void*)hc_play_close, (void*)hc_play_process,
	"http-ctl"
};
