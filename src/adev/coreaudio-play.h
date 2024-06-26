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

	struct coraud_out *c = phi_track_allocT(t, struct coraud_out);
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
	phi_track_free(t, c);
}

static int coraud_create(struct coraud_out *c, phi_track *t)
{
	audio_out *a = &c->out;
	int r;

	r = audio_out_open(a, t, &t->oaudio.format);
	if (r == FFAUDIO_EFORMAT) {
		t->oaudio.conv_format.interleaved = 1;
		return PHI_MORE;
	} else if (r != 0)
		return PHI_ERR;

	ffcoreaudio.dev_free(a->dev);
	a->dev = NULL;

	dbglog("%s buffer %ums, %uHz"
		, "opened", a->buffer_length_msec
		, t->oaudio.format.rate);

	t->oaudio.adev_ctx = a;
	t->oaudio.adev_stop = audio_stop;

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
		r = coraud_create(a, t);
		if (r == PHI_ERR) {
			return PHI_ERR;

		} else if (r == PHI_MORE) {
			if (a->state == 1) {
				errlog(t, "need input audio conversion");
				return PHI_ERR;
			}
			a->state = 1;
			return PHI_MORE;
		}

		a->state = 2;

		if (!t->oaudio.format.interleaved) {
			t->oaudio.conv_format.interleaved = 1;
			return PHI_MORE;
		}
	}

	uint old_state = ~0U;
	r = audio_out_write(&c->out, t, &old_state);
	return r;
}

static const phi_filter phi_coreaudio_play = {
	coraud_open, coraud_close, coraud_write,
	"coreaudio-play"
};
