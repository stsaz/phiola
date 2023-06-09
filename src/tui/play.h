/** phiola: tui: play/convert mode filter
2015, Simon Zolin */

static int tui_setvol(tui_track *u, uint vol);


static void* tuiplay_open(phi_track *t)
{
	tui_track *u = ffmem_new(tui_track);
	u->seek_msec = -1;
	u->lastpos = (uint)-1;
	u->t = t;
	mod->curtrk = u;

	uint vol = (mod->mute) ? 0 : mod->vol;
	if (vol != 100)
		tui_setvol(u, vol);

	u->show_info = 1;
	return u;
}

static void tuiplay_close(void *ctx, phi_track *t)
{
	tui_track *u = ctx;

	if (u == mod->curtrk) {
		mod->curtrk = NULL;
	}
	ffvec_free(&u->buf);
	ffmem_free(u);
}

/** All printable */
const uint ffcharmask_printable[] = {
	0,
	            // ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!
	0xffffffff, // 1111 1111 1111 1111  1111 1111 1111 1111
	            // _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@
	0xffffffff, // 1111 1111 1111 1111  1111 1111 1111 1111
	            //  ~}| {zyx wvut srqp  onml kjih gfed cba`
	0x7fffffff, // 0111 1111 1111 1111  1111 1111 1111 1111
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff
};

static inline ffbool ffbit_testarr(const uint *ar, uint bit)
{
	return ffbit_test32(&ar[bit / 32], bit % 32);
}

/** Skip characters by mask.
@mask: uint[8] */
char* ffs_skip_mask(const char *buf, ffsize len, const uint *mask)
{
	for (ffsize i = 0;  i != len;  i++) {
		if (!ffbit_testarr(mask, (ffbyte)buf[i]))
			return (char*)buf + i;
	}
	return (char*)buf + len;
}

static void tui_addtags(tui_track *u, ffvec *buf)
{
	uint i = 0;
	ffstr name, val;
	while (mod->phi_metaif->list(&u->t->meta, &i, &name, &val, PHI_META_UNIQUE)) {
		ffsize nt = (name.len < 8) ? 2 : 1;
		const char *end = ffs_skip_mask(val.ptr, val.len, ffcharmask_printable);
		if (end != ffstr_end(&val))
			ffstr_setz(&val, "<binary data>");
		ffvec_addfmt(buf, "%S%*c%S\n", &name, nt, '\t', &val);
	}
}

static void tui_info(tui_track *u)
{
	uint64 total_time, tsize;
	uint tmsec;
	phi_track *t = u->t;
	struct phi_af *fmt = &t->audio.format;

	u->sample_rate = fmt->rate;
	u->sampsize = pcm_size1(fmt);

	total_time = (u->total_samples != ~0ULL) ? samples_to_msec(u->total_samples, u->sample_rate) : 0;
	tmsec = (uint)(total_time / 1000);
	u->total_time_sec = tmsec;

	tsize = (t->input.size != ~0ULL) ? t->input.size : 0;

	ffstr artist = {}, title = {};
	mod->phi_metaif->find(&t->meta, FFSTR_Z("artist"), &artist, 0);
	mod->phi_metaif->find(&t->meta, FFSTR_Z("title"), &title, 0);

	ffsize trkid = (t->qent != NULL) ? mod->queue->index(t->qent) + 1 : 1;

	u->buf.len = 0;
	ffvec_addfmt(&u->buf, "\n%s#%L%s "
		"\"%S - %S\" "
		"%s\"%s\"%s "
		"%.02FMB %u:%02u.%03u (%,U samples) %ukbps %s %s %uHz %s"
		, mod->color.index, trkid, mod->color.reset
		, &artist, &title
		, mod->color.filename, t->conf.ifile.name, mod->color.reset
		, (double)tsize / (1024 * 1024)
		, tmsec / 60, tmsec % 60, (uint)(total_time % 1000)
		, u->total_samples
		, (t->audio.bitrate + 500) / 1000
		, t->audio.decoder
		, pcm_fmt_str(fmt->format)
		, fmt->rate
		, pcm_channelstr(fmt->channels));

	if (t->video.width != 0) {
		ffvec_addfmt(&u->buf, "  Video: %s, %ux%u"
			, t->video.decoder, (int)t->video.width, (int)t->video.height);
	}

	ffvec_addfmt(&u->buf, "\n\n");

	if (t->conf.print_tags) {
		tui_addtags(u, &u->buf);
	}

	ffstderr_write(u->buf.ptr, u->buf.len);
	u->buf.len = 0;
}

static void tuiplay_seek(tui_track *u, uint cmd, void *udata)
{
	int64 pos = (uint64)u->lastpos * 1000;
	uint by;
	switch ((ffsize)udata & FFKEY_MODMASK) {
	case 0:
		by = SEEK_STEP;
		break;
	case FFKEY_ALT:
		by = SEEK_STEP_MED;
		break;
	case FFKEY_CTRL:
		by = SEEK_STEP_LARGE;
		break;
	default:
		return;
	}
	if (cmd == CMD_SEEKRIGHT)
		pos += by;
	else
		pos = ffmax(pos - by, 0);

	u->seek_msec = pos;
	u->t->audio.seek_req = 1;
	u->t->oaudio.clear = 1;
	dbglog(u->t, "seeking: %U", pos);
	core->track->wake(u->t);
}

static int tui_setvol(tui_track *u, uint vol)
{
	int db;
	if (vol <= 100)
		db = vol2db(vol, VOL_LO) * 100;
	else
		db = vol2db_inc(vol - 100, VOL_MAX - 100, VOL_HI) * 100;
	u->t->audio.gain_db = db;
	return db;
}

static void tuiplay_vol(tui_track *u, uint cmd)
{
	uint vol = 0;

	switch (cmd & ~FFKEY_MODMASK) {
	case CMD_VOLUP:
		vol = mod->vol = ffmin(mod->vol + VOL_STEP, VOL_MAX);
		mod->mute = 0;
		break;

	case CMD_VOLDOWN:
		vol = mod->vol = ffmax((int)mod->vol - VOL_STEP, 0);
		mod->mute = 0;
		break;

	case CMD_MUTE:
		mod->mute = !mod->mute;
		vol = (mod->mute) ? 0 : mod->vol;
		break;
	}

	int db = tui_setvol(u, vol);
	userlog(u->t, "Volume: %.02FdB", (double)db / 100);
}

static int tuiplay_process(void *ctx, phi_track *t)
{
	tui_track *u = ctx;
	uint64 playpos;
	uint playtime;

	if (u->show_info) {
		u->show_info = 0;
		u->total_samples = t->audio.total;
		u->played_samples = 0;
		tui_info(u);
	}

	if (u->seek_msec != -1) {
		t->audio.seek = u->seek_msec;
		u->seek_msec = -1;
		return PHI_MORE; // new seek request
	} else if (!(t->chain_flags & PHI_FFWD)) {
		return PHI_MORE; // going back without seeking
	} else if (t->data_in.len == 0 && !(t->chain_flags & PHI_FFIRST)) {
		return PHI_MORE; // waiting for audio data
	} else if (t->audio.seek != -1 && !t->audio.seek_req) {
		dbglog(u->t, "seek: done");
		t->audio.seek = ~0ULL; // prev. seek is complete
	}

	if (mod->curtrk_rec != NULL)
		goto done; //don't show playback bar while recording in another track

	playpos = t->audio.pos;
	if (playpos == ~0ULL)
		playpos = u->played_samples;
	playtime = (uint)(samples_to_msec(playpos, u->sample_rate) / 1000);
	if (playtime == u->lastpos) {
		goto done;
	}
	u->lastpos = playtime;

	if (u->total_samples == ~0ULL
		|| playpos >= u->total_samples) {

		u->buf.len = 0;
		ffvec_addfmt(&u->buf, "%*c%u:%02u"
			, (ffsize)u->nback, '\r'
			, playtime / 60, playtime % 60);

		goto print;
	}

	u->buf.len = 0;
	uint dots = mod->progress_dots;
	ffvec_addfmt(&u->buf, "%*c[%s%*c%s%*c] "
		"%s%u:%02u%s / %u:%02u"
		, (ffsize)u->nback, '\r'
		, mod->color.progress, (ffsize)(playpos * dots / u->total_samples), '=', mod->color.reset
		, (ffsize)(dots - (playpos * dots / u->total_samples)), '.'
		, mod->color.progress, playtime / 60, playtime % 60, mod->color.reset
		, u->total_time_sec / 60, u->total_time_sec % 60);

print:
	fffile_write(ffstderr, u->buf.ptr, u->buf.len);
	u->nback = 1;
	if (core->conf.log_level >= PHI_LOG_DEBUG)
		u->nback = 0;
	u->buf.len = 0;

done:
	uint n = FFINT_DIVSAFE(t->data_in.len, u->sampsize);
	u->played_samples += n;
	dbglog(u->t, "samples: +%L [%U] @%U"
		, n, u->played_samples, t->audio.pos);

	t->data_out = t->data_in;

	if (t->chain_flags & PHI_FFIRST) {
		fffile_write(ffstderr, "\n", 1);
		return PHI_DONE;
	}
	return PHI_DATA;
}

static void tui_op_trk(struct tui_track *u, uint cmd)
{
	switch (cmd) {
	case CMD_SHOWTAGS:
		u->buf.len = 0;
		tui_addtags(u, &u->buf);
		ffstderr_write(u->buf.ptr, u->buf.len);
		u->buf.len = 0;
		break;
	}
}

static void tuiplay_pause_resume(tui_track *u)
{
	if (u->paused) {
		u->paused = 0;
		uint p = u->t->oaudio.pause;
		u->t->oaudio.pause = 0;
		dbglog(u->t, "unpausing");
		if (!p)
			core->track->wake(u->t);
		return;
	}

	u->t->oaudio.pause = 1;
	u->paused = 1;
	dbglog(u->t, "pausing");
}

static void tuiplay_rm(tui_track *u)
{
	if (!u->t) return;

	mod->queue->remove(u->t->qent);
	infolog(NULL, "Removed track from playlist");
}

static void tuiplay_rm_playnext(tui_track *u)
{
	if (!u->t) return;

	tuiplay_rm(u);
	cmd_next();
}

static const phi_filter phi_tuiplay = {
	tuiplay_open, tuiplay_close, tuiplay_process,
	"tui"
};
