/** phiola: WASAPI
2015, Simon Zolin */

typedef struct was_in {
	audio_in in;
	phi_timer tmr;
	struct phi_woeh_task task;
	uint latcorr;
} was_in;

static void wasapi_in_close(void *ctx, phi_track *t)
{
	was_in *wi = ctx;
	audio_in *a = &wi->in;
	core->timer(t->worker, &wi->tmr, 0, NULL, NULL);
	audio_in_close(a);
	ffmem_free(wi);
}

static void* wasapi_in_open(phi_track *t)
{
	if (0 != wasapi_init(t))
		return PHI_OPEN_ERR;

	was_in *wi = ffmem_new(was_in);
	audio_in *a = &wi->in;
	a->audio = &ffwasapi;
	a->trk = t;
	a->loopback = t->conf.iaudio.loopback;
	a->aflags = (t->conf.iaudio.exclusive) ? FFAUDIO_O_EXCLUSIVE | FFAUDIO_O_USER_EVENTS : 0;
	a->aflags |= FFAUDIO_O_UNSYNC_NOTIFY;
	if (0 != audio_in_open(a, t))
		goto fail;

	if (a->event_h)
		core->woeh(t->worker, a->event_h, &wi->task, audio_oncapt, a);
	else
		core->timer(t->worker, &wi->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
	return wi;

fail:
	wasapi_in_close(wi, t);
	return PHI_OPEN_ERR;
}

static int wasapi_in_read(void *ctx, phi_track *t)
{
	was_in *wi = ctx;
	return audio_in_read(&wi->in, t);
}

static const phi_filter phi_wasapi_rec = {
	wasapi_in_open, wasapi_in_close, wasapi_in_read,
	"wasapi-rec"
};
