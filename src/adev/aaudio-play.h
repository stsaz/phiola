/** phiola: AAudio playback
2024, Simon Zolin */

static void* aao_open(phi_track *t)
{
	audio_out *a = phi_track_allocT(t, audio_out);
	a->trk = t;
	a->audio = &ffaaudio;
	return a;
}

static void aao_close(audio_out *a, phi_track *t)
{
	if (a->err_code) {
		aa_buf_close(NULL);
		core->timer(t->worker, &mod->tmr, 0, NULL, NULL);
		if (mod->user == a)
			mod->user = NULL;

	} else if (mod->user == a) {
		dbglog(NULL, "stop");
		if (!!ffaaudio.stop(mod->abuf))
			errlog(t, "stop(): %s", ffaaudio.error(mod->abuf));
		core->timer(t->worker, &mod->tmr, -ABUF_CLOSE_WAIT, aa_buf_close, NULL);
		mod->user = NULL;
	}

	phi_track_free(t, a);
}

static int aao_create(audio_out *a, phi_track *t)
{
	int r;
	uint reused = 0;

	if (mod->abuf) {

		core->timer(t->worker, &mod->tmr, 0, NULL, NULL); // stop 'aa_buf_close' timer

		audio_out *cur = mod->user;
		if (cur) {
			mod->user = NULL;
			audio_out_stop(cur);
		}

		if (af_eq(&t->oaudio.format, &mod->fmt)) {
			dbglog(NULL, "stop/clear");
			ffaaudio.stop(mod->abuf);
			ffaaudio.clear(mod->abuf);
			a->stream = mod->abuf;

			reused = 1;
			goto done;
		}

		aa_buf_close(NULL);
	}

	r = audio_out_open(a, t, &t->oaudio.format);
	if (r == FFAUDIO_EFORMAT) {
		t->oaudio.conv_format.interleaved = 1;
		return PHI_MORE;
	} else if (r != 0) {
		return PHI_ERR;
	}

	mod->abuf = a->stream;
	mod->buf_len_msec = a->buffer_length_msec;
	mod->fmt = t->oaudio.format;

done:
	dbglog(t, "%s buffer %ums, %s/%uHz/%u"
		, (reused) ? "reused" : "opened", mod->buf_len_msec
		, phi_af_name(mod->fmt.format), mod->fmt.rate, mod->fmt.channels);

	t->oaudio.adev_ctx = a;
	t->oaudio.adev_stop = audio_stop;

	core->timer(t->worker, &mod->tmr, mod->buf_len_msec / 2, audio_out_onplay, a);
	mod->user = a;
	return PHI_DONE;
}

static int aao_write(void *ctx, phi_track *t)
{
	audio_out *a = ctx;
	int r;

	switch (a->state) {
	case 0:
	case 1:
		r = aao_create(a, t);
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
			core->timer(t->worker, &mod->tmr, mod->buf_len_msec / 2, audio_out_onplay, a);
	}
	return r;
}

static const phi_filter phi_aaudio_play = {
	aao_open, (void*)aao_close, (void*)aao_write,
	"aaudio-play"
};
