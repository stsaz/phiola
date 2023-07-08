/** phiola: CoreAudio
2018, Simon Zolin */

struct coraud_in {
	audio_in in;
	phi_timer tmr;
};

static void* coraud_in_open(phi_track *d)
{
	if (0 != mod_init(d->trk))
		return PHI_OPEN_ERR;

	struct coraud_in *c = ffmem_new(struct coraud_in);
	audio_in *a = &c->in;
	a->audio = &ffcoreaudio;
	a->trk = d->trk;

	if (0 != audio_in_open(a, d))
		goto fail;

	core->timer(&c->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
	return c;

fail:
	coraud_in_close(c);
	return PHI_OPEN_ERR;
}

static void coraud_in_close(void *ctx, phi_track *t)
{
	struct coraud_in *c = ctx;
	core->timer(&c->tmr, 0, NULL, NULL);
	audio_in_close(&c->in);
	ffmem_free(c);
}

static int coraud_in_read(void *ctx, phi_track *d)
{
	struct coraud_in *c = ctx;
	return audio_in_read(&c->in, d);
}

static const phi_filter phi_coreaudio_rec = {
	coraud_in_open, coraud_in_close, coraud_in_read,
	"coreaudio-rec"
};
