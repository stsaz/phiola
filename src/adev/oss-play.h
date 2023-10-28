/** phiola: OSS
2017, Simon Zolin */

static void* oss_open(phi_track *t)
{
	if (0 != mod_init(t->trk))
		return PHI_OPEN_ERR;

	audio_out *a = ffmem_new(audio_out);
	a->audio = &ffoss;
	a->trk = t->trk;
	return a;
}

static void oss_close(void *ctx, phi_track *t)
{
	audio_out *a = ctx;

	if (a->err_code != 0) {
		ffoss.free(mod->out);
		mod->out = NULL;
		core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
		if (mod->usedby == a)
			mod->usedby = NULL;

	} else if (mod->usedby == a) {
		if (a->fx->flags & PHI_FSTOP) {
			ffoss.free(mod->out);
			mod->out = NULL;

		} else {
			if (0 != ffoss.stop(mod->out))
				errlog(t, "stop: %s", ffoss.error(mod->out));
			ffoss.clear(mod->out);
		}

		core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
		mod->usedby = NULL;
	}

	ffoss.dev_free(a->dev);
	ffmem_free(a);
}

static int oss_create(audio_out *a, phi_track *t)
{
	struct phi_af fmt;
	int r, reused = 0;

	fmt = t->oaudio.format;

	if (mod->out != NULL) {

		audio_out *cur = mod->usedby;
		if (cur != NULL) {
			mod->usedby = NULL;
			core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
			audio_out_onplay(cur);
		}

		if (fmt.channels == mod->fmt.channels
			&& fmt.format == mod->fmt.format
			&& fmt.rate == mod->fmt.rate
			&& a->dev_idx == mod->dev_idx) {

			ffoss.stop(mod->out);
			ffoss.clear(mod->out);
			a->stream = mod->out;

			ffoss.dev_free(a->dev);
			a->dev = NULL;

			reused = 1;
			goto fin;
		}

		ffoss.free(mod->out);
		mod->out = NULL;
	}

	r = audio_out_open(a, t, &fmt);
	if (r == FFAUDIO_EFORMAT) {
		return PHI_MORE;
	} else if (r != 0)
		return PHI_ERR;

	ffoss.dev_free(a->dev);
	a->dev = NULL;

	mod->out = a->stream;
	mod->buffer_length_msec = a->buffer_length_msec;
	mod->fmt = fmt;
	mod->dev_idx = a->dev_idx;

fin:
	dbglog(t, "%s buffer %ums, %uHz"
		, reused ? "reused" : "opened", mod->buffer_length_msec
		, fmt.rate);

	mod->usedby = a;
	t->oaudio.adev_ctx = a;
	t->oaudio.adev_clear = audio_clear;

	core->timer(t->worker, &mod->tmr, mod->buffer_length_msec / 2, audio_out_onplay, a);
	return 0;
}

static int oss_write(void *ctx, phi_track *t)
{
	audio_out *a = ctx;
	int r;

	switch (a->state) {
	case 0:
	case 1:
		a->try_open = (a->state == 0);
		if (PHI_ERR == (r = oss_create(a, t)))
			return PHI_ERR;

		if (!(r == PHI_DONE && t->oaudio.format.interleaved)) {
			t->oaudio.conv_format.interleaved = 1;
			if (a->state == 1) {
				errlog(t, "need input audio conversion");
				return PHI_ERR;
			}
			a->state = 1;
			return PHI_MORE;
		}

		a->state = 2;
	}

	r = audio_out_write(a, t);
	return r;
}

static const phi_filter phi_oss_play = {
	oss_open, oss_close, oss_write,
	"oss-play"
};
