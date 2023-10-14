/** phiola: ALSA playback
2015, Simon Zolin */

static void* alsa_open(phi_track *t)
{
	if (0 != alsa_init(t))
		return PHI_OPEN_ERR;

	audio_out *a = ffmem_new(audio_out);
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
	ffmem_free(a);
}

static int alsa_create(audio_out *a, phi_track *t)
{
	struct phi_af fmt;
	int r, reused = 0;

	a->dev_idx = t->conf.oaudio.device_index;

	fmt = t->oaudio.format;
	a->aflags = FFAUDIO_O_HWDEV; // try "hw" device first, then fall back to "plughw"

	if (mod->out != NULL) {

		core->timer(t->worker, &mod->tmr, 0, NULL, NULL); // stop 'alsa_buf_close' timer

		audio_out *cur = mod->usedby;
		if (cur != NULL) {
			mod->usedby = NULL;
			audio_out_onplay(cur);
		}

		// Note: we don't support cases when devices are switched
		if (mod->dev_idx == a->dev_idx) {
			if (af_eq(&fmt, &mod->fmt)) {
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
			if (a->try_open && NULL != (good_fmt = fmt_conv_find(&mod->fmts, &fmt))
				&& af_eq(good_fmt, &mod->fmt)) {
				// Don't try to reopen the buffer, because it's likely to fail again.
				// Instead, just use the format ffaudio set for us previously.
				t->oaudio.conv_format = *good_fmt;
				return PHI_MORE;
			}
		}

		alsa_buf_close(NULL);
	}

	r = audio_out_open(a, t, &fmt);
	if (r == FFAUDIO_EFORMAT) {
		fmt_conv_add(&mod->fmts, &fmt, &t->oaudio.format);
		return PHI_MORE;
	} else if (r != 0) {
		a->err_code = r;
		return PHI_ERR;
	}

	ffalsa.dev_free(a->dev);
	a->dev = NULL;

	mod->out = a->stream;
	mod->buffer_length_msec = a->buffer_length_msec;
	mod->fmt = fmt;
	mod->dev_idx = a->dev_idx;

fin:
	mod->usedby = a;
	t->adev_ctx = a;
	t->adev_clear = audio_clear;
	dbglog(t, "%s buffer %ums, %s/%uHz/%u"
		, reused ? "reused" : "opened", mod->buffer_length_msec
		, pcm_format_str(mod->fmt.format), mod->fmt.rate, mod->fmt.channels);

	core->timer(t->worker, &mod->tmr, mod->buffer_length_msec / 2, audio_out_onplay, a);
	return PHI_DONE;
}

static int alsa_write(audio_out *a, phi_track *t)
{
	int r;

	switch (a->state) {
	case 0:
	case 1:
		a->try_open = (a->state == 0);
		if (PHI_ERR == (r = alsa_create(a, t)))
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

static const phi_filter phi_alsa_play = {
	alsa_open, (void*)alsa_close, (void*)alsa_write,
	"alsa-play"
};
