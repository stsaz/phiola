/** phiola: tui: recording mode filter
2015, Simon Zolin */

static void* tuirec_open(phi_track *t)
{
	struct tui_rec *u = ffmem_new(struct tui_rec);
	u->t = t;
	mod->curtrk_rec = u;
	u->maxdb = -MINDB;

	struct phi_af *f = &t->audio.format;
	u->sample_rate = f->rate;
	u->sampsize = pcm_size1(f);

	userlog(t, "Recording...  Source: %s %uHz %s.  Press \"s\" to stop."
		, phi_af_name(f->format), f->rate, pcm_channelstr(f->channels));
	return u;
}

static void tuirec_close(void *ctx, phi_track *t)
{
	struct tui_rec *u = ctx;
	if (u == mod->curtrk_rec)
		mod->curtrk_rec = NULL;
	ffvec_free(&u->buf);
	ffmem_free(u);
}

static int tuirec_process(void *ctx, phi_track *t)
{
	struct tui_rec *u = ctx;
	uint playtime;

	double db = t->audio.maxpeak_db;
	if (u->maxdb < db)
		u->maxdb = db;

	playtime = samples_to_msec(t->audio.pos, u->sample_rate);
	if (playtime / REC_STATUS_UPDATE == u->lastpos / REC_STATUS_UPDATE)
		goto done;
	u->lastpos = playtime;
	playtime /= 1000;

	if (db < -MINDB)
		db = -MINDB;

	ffsize pos = ((MINDB + db) / MINDB) * 10;
	u->buf.len = 0;
	ffvec_addfmt(&u->buf, "%*c%u:%02u  [%*c%*c] %3.02FdB / %.02FdB  "
		, (ffsize)u->nback, '\r'
		, playtime / 60, playtime % 60
		, pos, '='
		, (ffsize)(10 - pos), '.'
		, db, u->maxdb);

	fffile_write(ffstderr, u->buf.ptr, u->buf.len);
	u->nback = 1;
	if (core->conf.log_level >= PHI_LOG_DEBUG)
		u->nback = 0;
	u->buf.len = 0;

done:
	u->processed_samples += t->data_in.len / u->sampsize;
	dbglog(u->t, "samples: @%U +%L [%U]"
		, t->audio.pos, t->data_in.len / u->sampsize, u->processed_samples);

	t->data_out = t->data_in;

	if (t->chain_flags & PHI_FFIRST) {
		fffile_write(ffstderr, "\n", 1);
		return PHI_DONE;
	}
	return PHI_OK;
}

static void tuirec_pause_resume(struct tui_rec *u)
{
	if (u->paused) {
		u->paused = 0;
		core->track->wake(u->t);
		return;
	}
	u->paused = 1;
	infolog(u->t, "Recording is paused");
}

static const phi_filter phi_tuirec = {
	tuirec_open, tuirec_close, tuirec_process,
	"tui-rec"
};
