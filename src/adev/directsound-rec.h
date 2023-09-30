/** phiola: Direct Sound
2015, Simon Zolin */

typedef struct dsnd_in {
	audio_in in;
	phi_timer tmr;
} dsnd_in;

static void dsnd_in_close(void *ctx, phi_track *t)
{
	dsnd_in *ds = ctx;
	core->timer(t->worker, &ds->tmr, 0, NULL, NULL);
	audio_in_close(&ds->in);
}

static void* dsnd_in_open(phi_track *t)
{
	dsnd_in *ds = ffmem_new(dsnd_in);
	audio_in *a = &ds->in;
	a->audio = &ffdsound;
	a->trk = t;

	if (0 != audio_in_open(a, t))
		goto fail;

	core->timer(t->worker, &ds->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
	return ds;

fail:
	dsnd_in_close(ds, t);
	return PHI_OPEN_ERR;
}

static int dsnd_in_read(void *ctx, phi_track *t)
{
	dsnd_in *a = ctx;
	return audio_in_read(&a->in, t);
}

static const phi_filter phi_directsound_rec = {
	dsnd_in_open, dsnd_in_close, dsnd_in_read,
	"direct-sound-rec"
};
