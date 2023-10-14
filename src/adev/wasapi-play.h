/** phiola: WASAPI
2015, Simon Zolin */

static void* wasapi_open(phi_track *t)
{
	if (!ffsz_eq(t->data_type, "pcm")) {
		errlog(t, "unsupported input data type: %s", t->data_type);
		return PHI_OPEN_ERR;
	}

	if (0 != wasapi_init(t))
		return PHI_OPEN_ERR;

	audio_out *w = ffmem_new(audio_out);
	w->audio = &ffwasapi;
	w->trk = t;
	return w;
}

static int wasapi_create(audio_out *w, phi_track *t)
{
	struct phi_af fmt;
	int r, reused = 0;

	w->dev_idx = t->conf.oaudio.device_index;
	w->handle_dev_offline = (!t->conf.oaudio.device_index);

	fmt = t->oaudio.format;

	if (mod->out != NULL) {

		core->timer(t->worker, &mod->tmr, 0, NULL, NULL); // stop 'wasapi_close_tmr' timer

		audio_out *cur = mod->usedby;
		if (cur != NULL) {
			mod->usedby = NULL;
			audio_out_onplay(cur);
		}

		// Note: we don't support cases when devices are switched
		if (mod->dev_idx == w->dev_idx && mod->excl == t->conf.oaudio.exclusive) {
			if (af_eq(&fmt, &mod->fmt)) {
				dbglog(NULL, "stop/clear");
				ffwasapi.stop(mod->out);
				ffwasapi.clear(mod->out);
				w->stream = mod->out;

				ffwasapi.dev_free(w->dev);
				w->dev = NULL;

				reused = 1;
				goto fin;
			}

			const struct phi_af *good_fmt;
			if (w->try_open && NULL != (good_fmt = fmt_conv_find(&mod->fmts, &fmt))
				&& af_eq(good_fmt, &mod->fmt)) {
				// Don't try to reopen the buffer, because it's likely to fail again.
				// Instead, just use the format ffaudio set for us previously.
				t->oaudio.conv_format = *good_fmt;
				return PHI_MORE;
			}
		}

		wasapi_buf_close();
	}

	w->aflags |= (t->conf.oaudio.exclusive) ? FFAUDIO_O_EXCLUSIVE | FFAUDIO_O_USER_EVENTS : 0;
	w->aflags |= FFAUDIO_O_UNSYNC_NOTIFY;
	r = audio_out_open(w, t, &fmt);
	if (r == FFAUDIO_EFORMAT) {
		fmt_conv_add(&mod->fmts, &fmt, &t->oaudio.format);
		return PHI_MORE;
	} else if (r != 0)
		return PHI_ERR;

	ffwasapi.dev_free(w->dev);
	w->dev = NULL;

	mod->out = w->stream;
	mod->buffer_length_msec = w->buffer_length_msec;
	mod->excl = t->conf.oaudio.exclusive;
	mod->fmt = fmt;
	mod->dev_idx = w->dev_idx;

fin:
	mod->usedby = w;
	t->adev_ctx = w;
	t->adev_clear = audio_clear;
	dbglog(t, "%s buffer %ums, %s/%uHz/%u, exclusive:%u"
		, reused ? "reused" : "opened", mod->buffer_length_msec
		, pcm_format_str(mod->fmt.format), mod->fmt.rate, mod->fmt.channels
		, t->conf.oaudio.exclusive);

	if (!!w->event_h)
		core->woeh(t->worker, w->event_h, &w->tsk, audio_out_onplay, w);
	else
		core->timer(t->worker, &mod->tmr, mod->buffer_length_msec / 2, audio_out_onplay, w);
	return PHI_DONE;
}

void wasapi_close_tmr(void *param)
{
	wasapi_buf_close();
}

static void wasapi_close(void *ctx, phi_track *t)
{
	audio_out *w = ctx;
	if (mod->usedby == w) {
		if (0 != ffwasapi.stop(mod->out))
			errlog(w->trk, "stop: %s", ffwasapi.error(mod->out));
		if (t->chain_flags & PHI_FSTOP) {
			core->timer(t->worker, &mod->tmr, -ABUF_CLOSE_WAIT, wasapi_close_tmr, NULL);
		} else {
			core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
		}

		mod->usedby = NULL;
	}

	ffwasapi.dev_free(w->dev);
	ffmem_free(w);
}

static int wasapi_write(void *ctx, phi_track *t)
{
	audio_out *w = ctx;
	int r;

	switch (w->state) {
	case ST_TRY:
	case ST_OPEN:
		w->try_open = (w->state == 0);
		if (PHI_ERR == (r = wasapi_create(w, t)))
			return PHI_ERR;

		if (!(r == PHI_DONE && t->oaudio.format.interleaved)) {
			t->oaudio.conv_format.interleaved = 1;
			if (w->state == ST_OPEN) {
				errlog(t, "need input audio conversion");
				return PHI_ERR;
			}
			w->state = ST_OPEN;
			return PHI_MORE;
		}

		w->state = ST_WAITING;
	}

	r = audio_out_write(w, t);
	if (r == PHI_ERR) {
		wasapi_buf_close();
		core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
		mod->usedby = NULL;
		if (w->err_code == FFAUDIO_EDEV_OFFLINE && w->dev_idx == 0) {
			/*
			This code works only because shared mode WASAPI has the same audio format for all devices
			 so we won't request a new format conversion.
			For exclusive mode we need to handle new format conversion properly which isn't that easy to do.
			*/
			w->state = ST_OPEN;
			return wasapi_write(w, t);
		}
		return PHI_ERR;
	}
	return r;
}

static const phi_filter phi_wasapi_play = {
	wasapi_open, wasapi_close, wasapi_write,
	"wasapi-play"
};
