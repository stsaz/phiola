/** phiola: record via JACK
2020, Simon Zolin */

struct jack_in {
	audio_in in;
	phi_timer tmr;
};

static void jack_in_close(struct jack_in *ji, phi_track *t)
{
	core->timer(t->worker, &ji->tmr, 0, NULL, NULL);
	audio_in_close(&ji->in);
	phi_track_free(t, ji);
}

static void* jack_in_open(phi_track *t)
{
	if (0 != jack_initonce(t))
		return PHI_OPEN_ERR;

	struct jack_in *ji = phi_track_allocT(t, struct jack_in);
	audio_in *a = &ji->in;
	a->audio = &ffjack;
	a->trk = t;

	if (0 != audio_in_open(a, t))
		goto fail;

	core->timer(t->worker, &ji->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
	return ji;

fail:
	jack_in_close(ji, t);
	return PHI_OPEN_ERR;
}

static int jack_in_read(struct jack_in *ji, phi_track *t)
{
	return audio_in_read(&ji->in, t);
}

static const phi_filter phi_jack_rec = {
	jack_in_open, (void*)jack_in_close, (void*)jack_in_read,
	"jack-rec"
};
