/** phiola: play via PulseAudio
2017, Simon Zolin */

static void* pulse_open(phi_track *t)
{
	if (0 != pulse_init(t))
		return PHI_OPEN_ERR;

	audio_out *a = phi_track_allocT(t, audio_out);
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
	phi_track_free(t, a);
}

static int pulse_create(audio_out *a, phi_track *t)
{
	int r, reused = 0;
	a->dev_idx = t->conf.oaudio.device_index;

	if (mod->out != NULL) {

		core->timer(t->worker, &mod->tmr, 0, NULL, NULL); // stop 'pulse_close_tmr' timer

		audio_out *cur = mod->usedby;
		if (cur != NULL) {
			mod->usedby = NULL;
			audio_out_stop(cur);
		}

		if (af_eq(&t->oaudio.format, &mod->fmt)
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

	while (0 != (r = audio_out_open(a, t, &t->oaudio.format))) {
		if (r == FFAUDIO_EFORMAT) {
			t->oaudio.conv_format.interleaved = 1;
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
	mod->fmt = t->oaudio.format;
	mod->dev_idx = a->dev_idx;

fin:
	dbglog(t, "%s buffer %ums, %s/%uHz"
		, reused ? "reused" : "opened", mod->buffer_length_msec
		, phi_af_name(mod->fmt.format), mod->fmt.rate);

	mod->usedby = a;
	t->oaudio.adev_ctx = a;
	t->oaudio.adev_stop = audio_stop;

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
		r = pulse_create(a, t);
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
	if (old_state != ~0U) {
		if (a->state == ST_PAUSED)
			core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
		else if (old_state == ST_PAUSED)
			core->timer(t->worker, &mod->tmr, mod->buffer_length_msec / 2, audio_out_onplay, a);
	}
	return r;
}

static const phi_filter phi_pulse_play = {
	pulse_open, pulse_close, pulse_write,
	"pulse.play"
};
