/** phiola: ALSA playback
2015, Simon Zolin */

static void* alsa_open(phi_track *t)
{
	if (0 != alsa_init(t))
		return PHI_OPEN_ERR;

	audio_out *a = phi_track_allocT(t, audio_out);
	a->audio = &ffalsa;
	a->trk = t;
	return a;
}

static void alsa_close(audio_out *a, phi_track *t)
{
	if (a->err_code != 0) {
		alsa_buf_close(NULL);
		core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
		if (mod->usedby == a)
			mod->usedby = NULL;

	} else if (mod->usedby == a) {
		dbglog(NULL, "stop");
		if (0 != ffalsa.stop(mod->out))
			errlog(t, "stop(): %s", ffalsa.error(mod->out));
		core->timer(t->worker, &mod->tmr, -ABUF_CLOSE_WAIT, alsa_buf_close, NULL);
		mod->usedby = NULL;
	}

	ffalsa.dev_free(a->dev);
	phi_track_free(t, a);
}

static int alsa_create(audio_out *a, phi_track *t)
{
	int r, reused = 0;

	if (mod->out != NULL) {

		core->timer(t->worker, &mod->tmr, 0, NULL, NULL); // stop 'alsa_buf_close' timer

		audio_out *cur = mod->usedby;
		if (cur != NULL) {
			mod->usedby = NULL;
			audio_out_stop(cur);
		}

		// Note: we don't support cases when devices are switched
		if (mod->dev_idx == t->conf.oaudio.device_index) {
			if (af_eq(&t->oaudio.format, &mod->fmt)) {
				dbglog(NULL, "stop/clear");
				ffalsa.stop(mod->out);
				ffalsa.clear(mod->out);
				a->stream = mod->out;

				ffalsa.dev_free(a->dev);
				a->dev = NULL;

				reused = 1;
				goto fin;
			}

			const struct phi_af *good_fmt;
			if (a->try_open && NULL != (good_fmt = fmt_conv_find(&mod->fmts, &t->oaudio.format))
				&& af_eq(good_fmt, &mod->fmt)) {
				// Don't try to reopen the buffer, because it's likely to fail again.
				// Instead, just use the format ffaudio set for us previously.
				t->oaudio.conv_format = *good_fmt;
				t->oaudio.conv_format.interleaved = 1;
				return PHI_MORE;
			}
		}

		alsa_buf_close(NULL);
	}

	a->aflags = FFAUDIO_O_HWDEV; // try "hw" device first, then fall back to "plughw"
	r = audio_out_open(a, t, &t->oaudio.format);
	if (r == FFAUDIO_EFORMAT) {
		t->oaudio.conv_format.interleaved = 1;
		struct phi_af req_fmt = t->oaudio.format;
		phi_af_update(&req_fmt, &t->oaudio.conv_format);
		fmt_conv_add(&mod->fmts, &t->oaudio.format, &req_fmt);
		return PHI_MORE;
	} else if (r != 0) {
		a->err_code = r;
		return PHI_ERR;
	}

	ffalsa.dev_free(a->dev);
	a->dev = NULL;

	mod->out = a->stream;
	mod->buffer_length_msec = a->buffer_length_msec;
	mod->fmt = t->oaudio.format;
	mod->dev_idx = a->dev_idx;

fin:
	mod->usedby = a;
	t->oaudio.adev_ctx = a;
	t->oaudio.adev_stop = audio_stop;
	dbglog(t, "%s buffer %ums, %s/%uHz/%u"
		, reused ? "reused" : "opened", mod->buffer_length_msec
		, phi_af_name(mod->fmt.format), mod->fmt.rate, mod->fmt.channels);

	core->timer(t->worker, &mod->tmr, mod->buffer_length_msec / 2, audio_out_onplay, a);
	return PHI_DONE;
}

/*
Only 1 device buffer is used at a time, even when several tracks are active.
Each new track forcefully stops the previous track and unassigns the buffer from it.

? Device is in use by another track:
	- Force stop the other track and unlink it from the device buffer
	? Same device index:
		? Same format:
			. Reuse device buffer
		? First try:
			? Format isn't supported:
				. Request audio conversion
	- Close previous device buffer
- Open device buffer
	? First try:
		? Format error:
			. Request audio conversion
. Begin writing data to device
*/
static int alsa_write(audio_out *a, phi_track *t)
{
	int r;

	switch (a->state) {
	case 0:
	case 1:
		a->try_open = (a->state == 0);
		r = alsa_create(a, t);
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
	r = audio_out_write(a, t, &old_state);
	return r;
}

static const phi_filter phi_alsa_play = {
	alsa_open, (void*)alsa_close, (void*)alsa_write,
	"alsa-play"
};
