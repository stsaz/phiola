/** phiola: TUI-ncurses: playback
2026, Simon Zolin */

struct tui2_play_trk {
	phi_track *trk;
	uint pos_last_sec, total_sec;
	int seek_delta_sec;
	uint seek_abs_sec;
	uint meta_change_seen :1;
	uint paused :1;
};

static void* tui2_play_open(phi_track *t)
{
	struct tui2_play_trk *p = phi_track_allocT(t, struct tui2_play_trk);
	p->trk = t;
	p->pos_last_sec = ~0U;
	if (mod->volume_db)
		t->oaudio.gain_db = mod->volume_db;
	tui2_play_started(p, t);
	return p;
}

static void tui2_play_close(void *f, phi_track *t)
{
	struct tui2_play_trk *p = f;
	tui2_play_finished(p);
	phi_track_free(t, p);
}

static void play_title(struct tui2_play_trk *p)
{
	const phi_track *t = p->trk;
	const struct phi_queue_entry *qe = t->qent;

	ffstr artist = {}, title = {};
	core->metaif->find(&p->trk->meta, FFSTR_Z("artist"), &artist, 0);
	core->metaif->find(&p->trk->meta, FFSTR_Z("title"), &title, 0);
	uint n;
	if (title.len) {
		n = tui2_printf("%S - %S", &artist, &title);
	} else {
		ffpath_split3_str(FFSTR_Z(qe->url), NULL, &title, NULL); // Use file name as title
		n = tui2_printf("%S", &title);
	}

	uint nbase = n;
	if (mod->play_info_title) {
		n = tui2_appendf(n, " [%u kbps, %s, %u Hz, %s, %s]"
			, (t->audio.bitrate + 500) / 1000
			, t->audio.decoder
			, t->audio.format.rate
			, phi_af_name(t->audio.format.format)
			, pcm_channelstr(t->audio.format.channels));
	}

	ffncurses_line_clear_x(&mod->wmain, Y_TITLE, X_TITLE);
	ffncurses_printn_attr(&mod->wmain, Y_TITLE, X_TITLE, mod->buf, n, 0, CLR_TITLE);

	n = tui2_appendf(nbase, " - phiola");
	ffstd_title(mod->buf, n);
}

static int play_seek(struct tui2_play_trk *p)
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

static void play_progress(struct tui2_play_trk *p)
{
	phi_track *t = p->trk;
	uint play_time = (t->audio.pos != ~0ULL) ? (uint)(samples_to_msec(t->audio.pos, t->audio.format.rate) / 1000) : 0;
	if (play_time == p->pos_last_sec)
		return;
	p->pos_last_sec = play_time;

	if (!p->total_sec && t->audio.total != ~0ULL)
		p->total_sec = (uint)(samples_to_msec(t->audio.total, t->audio.format.rate) / 1000);

	uint prog_cap = ffmax((int)ffncurses_width() - FFS_LEN("[] xx:xx / xx:xx"), 0);
	uint prog_pos = FFINT_DIVSAFE(play_time * prog_cap, p->total_sec);
	prog_pos = ffmin(prog_pos, prog_cap);
	uint n = tui2_printf("[%*c%*c] %u:%02u / "
		, (ffsize)prog_pos, '#'
		, (ffsize)(prog_cap - prog_pos), '-'
		, play_time / 60, play_time % 60
		);
	if (t->audio.total != ~0ULL) {
		n = tui2_appendf(n, "%u:%02u"
			, p->total_sec / 60, p->total_sec % 60);
	} else {
		n = tui2_appendf(n, "--");
	}

	ffncurses_line_clear(&mod->wmain, Y_PROGRESS);
	ffncurses_printn_attr(&mod->wmain, Y_PROGRESS, 0, mod->buf, n, 0, 0);
}

static int tui2_play_process(void *f, phi_track *t)
{
	struct tui2_play_trk *p = f;

	if (t->chain_flags & PHI_FSTOP)
		return PHI_FIN;

	uint new_meta = (t->meta_changed && !p->meta_change_seen);
	p->meta_change_seen = t->meta_changed;
	if (new_meta) {
		if (!t->audio.format.rate) {
			errlog("audio sample rate is not set");
			return PHI_ERR;
		}
		play_title(p);
	}

	play_progress(p);
	ffncurses_update(&mod->wmain);

	if (play_seek(p))
		return PHI_MORE;

	t->data_out = t->data_in;
	return !(t->chain_flags & PHI_FFIRST) ? PHI_DATA : PHI_DONE;
}

static void tui2_play_pause(struct tui2_play_trk *p)
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

	tui2_status((unpause) ? "" : "Paused");
}

static void tui2_play_volume(struct tui2_play_trk *p)
{
	if (!p) return;
	p->trk->oaudio.gain_db = mod->volume_db;
}

static void tui2_play_seek(struct tui2_play_trk *p, uint abs, int delta)
{
	if (!p) return;
	if (abs != ~0U)
		p->seek_abs_sec = abs;
	else
		p->seek_delta_sec += delta;

	p->trk->audio.seek_req = 1;
	p->trk->oaudio.clear = 1;
	if (p->trk->oaudio.adev_ctx)
		p->trk->oaudio.adev_stop(p->trk->oaudio.adev_ctx);
	core->track->wake(p->trk);
}

#define INFO_N_FIXED  2

static void play_info_display()
{
	struct tui2_play_trk *p = mod->playing;
	if (!p) return;
	const phi_track *t = p->trk;
	const struct phi_queue_entry *qe = t->qent;
	uint y = 1, n, i = mod->dlg.top, it = 0;
	ffstr name, val;

	// Skip scrolled rows
	for (uint mi = 0;  INFO_N_FIXED + mi < i;  mi++) {
		if (!core->metaif->list(&qe->meta, &it, &name, &val, 0))
			break;
	}

	while (y <= mod->wpopup.ch) {
		switch (i++) {
		case 0:
			n = tui2_printf("url : %s", qe->url);
			break;

		case 1:
			n = tui2_printf("info : %u kbps, %s, %u Hz, %s, %s"
				, (t->audio.bitrate + 500) / 1000
				, t->audio.decoder
				, t->audio.format.rate
				, phi_af_name(t->audio.format.format)
				, pcm_channelstr(t->audio.format.channels));
			break;

		default:
			if (!core->metaif->list(&qe->meta, &it, &name, &val, 0))
				goto end;
			n = tui2_printf("%S : %S"
				, &name, &val);
		}

		ffncurses_println_attr(&mod->wpopup, y++, 1, mod->buf, n, 0, 0);
	}

end:
	while (y <= mod->wpopup.ch) {
		ffncurses_line_clear(&mod->wpopup, y++);
	}
}

/** Return 0 if handled */
static int play_info_action(int k)
{
	switch (k) {
	case FFKEY_UP:
		if (mod->dlg.top > 0)
			mod->dlg.top--;
		break;

	case FFKEY_DOWN: {
		const struct phi_queue_entry *qe = (mod->playing) ? mod->playing->trk->qent : NULL;
		if (qe && mod->dlg.top + mod->wpopup.ch < INFO_N_FIXED + (uint)META_LEN(&qe->meta))
			mod->dlg.top++;
		break;
	}

	default:
		return 1;
	}

	play_info_display();
	return 0;
}

static void play_info_show(struct tui2_play_trk *p)
{
	if (!p) return;

	tui2_popup("Info", 66);
	play_info_display();
	mod->popup_type = POPUP_PLAYINFO;
}

static const phi_filter tui2_if_play = {
	tui2_play_open, tui2_play_close, tui2_play_process,
	"tui2"
};
