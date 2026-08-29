/** phiola: CoreAudio
2018, Simon Zolin */

struct coreaudio_in {
	audio_in in;
	phi_timer tmr;
};

static void coreaudio_in_close(void *ctx, phi_track *t)
{
	struct coreaudio_in *c = ctx;
	core->timer(t->worker, &c->tmr, 0, NULL, NULL);
	audio_in_close(&c->in);
	phi_track_free(t, c);
}

static void* coreaudio_in_open(phi_track *t)
{
	if (mod_init(t))
		return PHI_OPEN_ERR;

	struct coreaudio_in *c = phi_track_allocT(t, struct coreaudio_in);
	audio_in *a = &c->in;
	a->audio = &ffcoreaudio;
	a->trk = t;

	if (audio_in_open(a, t))
		goto err;

	core->timer(t->worker, &c->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
	return c;

err:
	coreaudio_in_close(c, t);
	return PHI_OPEN_ERR;
}

static int coreaudio_in_read(void *ctx, phi_track *t)
{
	struct coreaudio_in *c = ctx;
	return audio_in_read(&c->in, t);
}

const phi_filter phi_coreaudio_rec = {
	coreaudio_in_open, coreaudio_in_close, coreaudio_in_read,
	"coreaudio-rec"
};
