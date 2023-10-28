/** phiola: CoreAudio
2018, Simon Zolin */

struct coraud_out {
	uint state;
	audio_out out;
	phi_timer tmr;
};

static void* coraud_open(phi_track *t)
{
	if (0 != mod_init(t->trk))
		return PHI_OPEN_ERR;

	struct coraud_out *c = ffmem_new(struct coraud_out);
	c->out.trk = t->trk;
	c->out.core = core;
	c->out.audio = &ffcoreaudio;
	c->out.track = t->track;
	c->out.trk = t->trk;
	return c;
}

static void coraud_close(void *ctx, phi_track *t)
{
	struct coraud_out *c = ctx;
	core->timer(t->worker, &c->tmr, 0, NULL, NULL);
	ffcoreaudio.free(c->out.stream);
	ffcoreaudio.dev_free(c->out.dev);
	ffmem_free(c);
}

static int coraud_create(struct coraud_out *c, phi_track *t)
{
	audio_out *a = &c->out;
	struct phi_af fmt;
	int r;

	fmt = t->oaudio.format;

	r = audio_out_open(a, t, &fmt);
	if (r == FFAUDIO_EFORMAT) {
		return PHI_MORE;
	} else if (r != 0)
		return PHI_ERR;

	ffcoreaudio.dev_free(a->dev);
	a->dev = NULL;

	dbglog("%s buffer %ums, %uHz"
		, "opened", a->buffer_length_msec
		, fmt.rate);

	t->oaudio.adev_ctx = a;
	t->oaudio.adev_clear = audio_clear;

	core->timer(t->worker, &c->tmr, a->buffer_length_msec / 2, audio_out_onplay, a);
	return PHI_DONE;
}

static int coraud_write(void *ctx, phi_track *t)
{
	struct coraud_out *c = ctx;
	int r;

	switch (a->state) {
	case 0:
	case 1:
		a->try_open = (a->state == 0);
		if (PHI_ERR == (r = coraud_create(a, t)))
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

	r = audio_out_write(&c->out, t);
	return r;
}

static const phi_filter phi_coreaudio_play = {
	coraud_open, coraud_close, coraud_write,
	"coreaudio-play"
};
