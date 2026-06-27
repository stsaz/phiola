/** phiola: TUI-ncurses
2026, Simon Zolin */

struct tui2_play_trk {
	phi_track *trk;
	uint pos_last_sec, total_sec;
	int seek_delta_sec;
	uint hdr :1;
	uint paused :1;
};

static void* tui2_play_open(phi_track *t)
{
	struct tui2_play_trk *p = phi_track_allocT(t, struct tui2_play_trk);
	p->trk = t;
	p->pos_last_sec = ~0U;
	mod->playing = p;
	return p;
}

static void tui2_play_close(void *f, phi_track *t)
{
	struct tui2_play_trk *p = f;
	if (p == mod->playing)
		mod->playing = NULL;
	phi_track_free(t, p);
}

static void play_title(struct tui2_play_trk *p)
{
	const struct phi_queue_entry *qe = p->trk->qent;

	char buf[500];
	uint w = ffmin(ffncurses_width(), sizeof(buf));
	ffstr artist = {}, title = {};
	core->metaif->find(&p->trk->meta, FFSTR_Z("artist"), &artist, 0);
	core->metaif->find(&p->trk->meta, FFSTR_Z("title"), &title, 0);
	int r;
	if (title.len) {
		r = ffs_format(buf, sizeof(buf), "φ %S - %S", &artist, &title);
	} else {
		ffpath_split3_str(FFSTR_Z(qe->url), NULL, &title, NULL); // Use file name as title
		r = ffs_format(buf, sizeof(buf), "φ %S", &title);
	}
	if (r < 0)
		r = -1;
	uint n = text_clamp(buf, r, w);
	ffncurses_line_clear(&mod->wmain, Y_TITLE);
	ffncurses_printn_attr(&mod->wmain, Y_TITLE, 0, buf, n, 0, 1);
}

static int play_seek(struct tui2_play_trk *p)
{
	phi_track *t = p->trk;
	if (p->seek_delta_sec) {
		t->audio.seek = samples_to_msec(t->audio.pos, t->audio.format.rate) + p->seek_delta_sec * 1000;
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

static void play_progress(struct tui2_play_trk *p)
{
	phi_track *t = p->trk;
	char buf[500];
	uint play_time = (uint)(samples_to_msec(t->audio.pos, t->audio.format.rate) / 1000);
	if (play_time == p->pos_last_sec)
		return;
	p->pos_last_sec = play_time;

	if (!p->total_sec)
		p->total_sec = (uint)(samples_to_msec(t->audio.total, t->audio.format.rate) / 1000);

	uint w = ffmin(ffncurses_width(), sizeof(buf));
	uint prog_cap = ffmax((int)w - FFS_LEN("[] xx:xx / xx:xx"), 0);
	uint prog_pos = play_time * prog_cap / p->total_sec;
	ffsz_format(buf, sizeof(buf),
		"[%*c%*c] %u:%02u / %u:%02u"
		, (ffsize)prog_pos, '#'
		, (ffsize)(prog_cap - prog_pos), '-'
		, play_time / 60, play_time % 60
		, p->total_sec / 60, p->total_sec % 60
		);

	ffncurses_println_attr(&mod->wmain, Y_PROGRESS, 0, buf, 0, 0);
}

static int tui2_play_process(void *f, phi_track *t)
{
	struct tui2_play_trk *p = f;

	if (t->chain_flags & PHI_FSTOP)
		return PHI_FIN;

	if (!t->audio.format.rate) {
		// Meta hasn't been read yet
		if (!(t->chain_flags & PHI_FFWD))
			return PHI_MORE;
		goto end;
	}

	if (!p->hdr) {
		p->hdr = 1;
		play_title(p);
	}

	play_progress(p);
	ffncurses_update(&mod->wmain);

	if (play_seek(p))
		return PHI_MORE;

end:
	t->data_out = t->data_in;
	return !(t->chain_flags & PHI_FFIRST) ? PHI_DATA : PHI_DONE;
}

static void tui2_play_pause(struct tui2_play_trk *p)
{
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

	ffncurses_line_clear(&mod->wmain, mod->y_status);
	if (!unpause)
		ffncurses_print_attr(&mod->wmain, mod->y_status, 0, "Paused", 0, CLR_TITLE);
	ffncurses_update(&mod->wmain);
}

#define VOL_MAX  125
#define VOL_LO  (-40)
#define VOL_HI  6

static void tui2_play_volume(struct tui2_play_trk *p)
{
	double db;
	if (mod->volume <= 100)
		db = vol2db(mod->volume, VOL_LO);
	else
		db = vol2db_inc(mod->volume - 100, VOL_MAX - 100, VOL_HI);
	p->trk->oaudio.gain_db = db;

	char buf[256];
	ffsz_format(buf, sizeof(buf), "Volume: %.02FdB", db);
	ffncurses_println_attr(&mod->wmain, mod->y_status, 0, buf, 0, 0);
}

static void tui2_play_seek(struct tui2_play_trk *p, int delta)
{
	p->seek_delta_sec += delta;

	p->trk->audio.seek_req = 1;
	p->trk->oaudio.clear = 1;
	if (p->trk->oaudio.adev_ctx)
		p->trk->oaudio.adev_stop(p->trk->oaudio.adev_ctx);
	core->track->wake(p->trk);
}

static const phi_filter tui2_if_play = {
	tui2_play_open, tui2_play_close, tui2_play_process,
	"tui2"
};
