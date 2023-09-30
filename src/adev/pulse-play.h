/** phiola: play via PulseAudio
2017, Simon Zolin */

static void* pulse_open(phi_track *t)
{
	if (0 != pulse_init(t))
		return PHI_OPEN_ERR;

	audio_out *a = ffmem_new(audio_out);
	a->audio = &ffpulse;
	a->trk = t;
	return a;
}

static void pulse_close_tmr(void *param)
{
	pulse_buf_close();
}

static void pulse_close(void *ctx, phi_track *t)
{
	audio_out *a = ctx;

	if (a->err_code != 0) {
		pulse_buf_close();
		core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
		if (mod->usedby == a)
			mod->usedby = NULL;

	} else if (mod->usedby == a) {
		if (0 != ffpulse.stop(mod->out))
			errlog(a->trk, "stop: %s", ffpulse.error(mod->out));
		core->timer(t->worker, &mod->tmr, -ABUF_CLOSE_WAIT, pulse_close_tmr, NULL);
		mod->usedby = NULL;
	}

	ffpulse.dev_free(a->dev);
	ffmem_free(a);
}

static int pulse_create(audio_out *a, phi_track *t)
{
	struct phi_af fmt;
	int r, reused = 0;

	a->dev_idx = t->conf.oaudio.device_index;

	fmt = t->oaudio.format;

	if (mod->out != NULL) {

		core->timer(t->worker, &mod->tmr, 0, NULL, NULL); // stop 'pulse_close_tmr' timer

		audio_out *cur = mod->usedby;
		if (cur != NULL) {
			mod->usedby = NULL;
			audio_out_onplay(cur);
		}

		if (af_eq(&fmt, &mod->fmt)
			&& a->dev_idx == mod->dev_idx) {

			dbglog(a->trk, "reuse buffer: ffpulse.stop/clear");
			ffpulse.stop(mod->out);
			ffpulse.clear(mod->out);
			a->stream = mod->out;

			ffpulse.dev_free(a->dev);
			a->dev = NULL;

			reused = 1;
			goto fin;
		}

		pulse_buf_close();
	}

	while (0 != (r = audio_out_open(a, t, &fmt))) {
		if (r == FFAUDIO_EFORMAT) {
			return PHI_MORE;

		} else if (r == FFAUDIO_ECONNECTION) {
			if (!a->reconnect) {
				dbglog(t, "reconnecting...");
				a->reconnect = 1;

				ffpulse.uninit();
				mod->init_ok = 0;

				if (0 != pulse_init(t)) {
					a->err_code = FFAUDIO_ERROR;
					return PHI_ERR;
				}
				continue;
			}
		}
		a->err_code = FFAUDIO_ERROR;
		return PHI_ERR;
	}

	ffpulse.dev_free(a->dev);
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

	core->timer(t->worker, &mod->tmr, mod->buffer_length_msec / 2, audio_out_onplay, a);
	return PHI_DONE;
}

static int pulse_write(void *ctx, phi_track *t)
{
	audio_out *a = ctx;
	int r;

	switch (a->state) {
	case 0:
	case 1:
		a->try_open = (a->state == 0);
		if (PHI_ERR == (r = pulse_create(a, t)))
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

static const phi_filter phi_pulse_play = {
	pulse_open, pulse_close, pulse_write,
	"pulse.play"
};
