/** phiola: CoreAudio
2018, Simon Zolin */

struct coreaudio_out {
	uint state;
	audio_out out;
	phi_timer tmr;
};

static void* coreaudio_open(phi_track *t)
{
	if (mod_init(t))
		return PHI_OPEN_ERR;

	struct coreaudio_out *c = phi_track_allocT(t, struct coreaudio_out);
	c->out.audio = &ffcoreaudio;
	c->out.trk = t;
	return c;
}

static void coreaudio_close(void *ctx, phi_track *t)
{
	struct coreaudio_out *c = ctx;
	core->timer(t->worker, &c->tmr, 0, NULL, NULL);
	ffcoreaudio.free(c->out.stream);
	ffcoreaudio.dev_free(c->out.dev);
	phi_track_free(t, c);
}

static int coreaudio_create(struct coreaudio_out *c, phi_track *t)
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

	dbglog(t, "%s buffer %ums, %uHz"
		, "opened", a->buffer_length_msec
		, t->oaudio.format.rate);

	t->oaudio.adev_ctx = a;
	t->oaudio.adev_stop = audio_stop;

	core->timer(t->worker, &c->tmr, a->buffer_length_msec / 2, audio_out_onplay, a);
	return PHI_DONE;
}

static int coreaudio_write(void *ctx, phi_track *t)
{
	struct coreaudio_out *c = ctx;
	audio_out *a = &c->out;
	int r;

	switch (a->state) {
	case ST_TRY:
	case ST_OPEN:
		a->try_open = (a->state == ST_TRY);
		r = coreaudio_create(c, t);
		if (r == PHI_ERR) {
			return PHI_ERR;

		} else if (r == PHI_MORE) {
			if (a->state == ST_OPEN) {
				errlog(t, "need input audio conversion");
				return PHI_ERR;
			}
			a->state = 1;
			return PHI_MORE;
		}

		a->state = ST_WAITING;

		if (!t->oaudio.format.interleaved) {
			t->oaudio.conv_format.interleaved = 1;
			return PHI_MORE;
		}
	}

	uint old_state = ~0U;
	r = audio_out_write(a, t, &old_state);
	return r;
}

const phi_filter phi_coreaudio_play = {
	coreaudio_open, coreaudio_close, coreaudio_write,
	"coreaudio-play"
};
