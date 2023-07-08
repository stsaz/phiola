/** phiola: WASAPI
2015, Simon Zolin */

typedef struct was_in {
	audio_in in;
	phi_timer tmr;
	uint latcorr;
} was_in;

static void wasapi_in_close(void *ctx, phi_track *t)
{
	was_in *wi = ctx;
	audio_in *a = &wi->in;
	core->timer(&wi->tmr, 0, NULL, NULL);
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

	//@ if () {
	// 	// use loopback device specified by user
	// 	a->loopback = 1;
	// }

	//@ if ()
	// 	a->aflags = FFAUDIO_O_EXCLUSIVE;

	a->aflags |= FFAUDIO_O_UNSYNC_NOTIFY;
	if (0 != audio_in_open(a, t))
		goto fail;

	core->timer(&wi->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
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
