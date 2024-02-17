/** phiola: Direct Sound
2015, Simon Zolin */

typedef struct dsnd_out {
	audio_out out;
	phi_timer tmr;
} dsnd_out;

static void* dsnd_open(phi_track *t)
{
	dsnd_out *ds = ffmem_new(dsnd_out);
	audio_out *a = &ds->out;
	a->audio = &ffdsound;
	a->trk = t;
	return ds;
}

static void dsnd_close(void *ctx, phi_track *t)
{
	dsnd_out *ds = ctx;
	ffdsound.dev_free(ds->out.dev);
	ffdsound.free(ds->out.stream);
	core->timer(t->worker, &ds->tmr, 0, NULL, NULL);
	ffmem_free(ds);
}

static int dsnd_create(dsnd_out *ds, phi_track *t)
{
	audio_out *a = &ds->out;
	int r;

	r = audio_out_open(a, t, &t->oaudio.format);
	if (r == FFAUDIO_EFORMAT) {
		t->oaudio.conv_format.interleaved = 1;
		return PHI_MORE;
	} else if (r != 0)
		return PHI_ERR;

	ffdsound.dev_free(a->dev);
	a->dev = NULL;

	dbglog(t, "%s buffer %ums, %uHz"
		, "opened", a->buffer_length_msec
		, t->oaudio.format.rate);

	t->oaudio.adev_ctx = a;
	t->oaudio.adev_stop = audio_stop;

	core->timer(t->worker, &ds->tmr, a->buffer_length_msec / 2, audio_out_onplay, a);
	return PHI_DONE;
}

static int dsnd_write(void *ctx, phi_track *t)
{
	dsnd_out *ds = ctx;
	audio_out *a = &ds->out;
	int r;

	switch (a->state) {
	case 0:
	case 1:
		a->try_open = (a->state == 0);
		r = dsnd_create(ds, t);
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

	r = audio_out_write(a, t);
	return r;
}

static const phi_filter phi_directsound_play = {
	dsnd_open, dsnd_close, dsnd_write,
	"direct-sound-play"
};
