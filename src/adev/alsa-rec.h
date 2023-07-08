/** phiola: record via ALSA
2015, Simon Zolin */

struct alsar {
	audio_in in;
	phi_timer tmr;
};

static void alsar_close(struct alsar *al, phi_track *t)
{
	core->timer(&al->tmr, 0, NULL, NULL);
	audio_in_close(&al->in);
	ffmem_free(al);
}

static void* alsar_open(phi_track *t)
{
	if (0 != alsa_init(t))
		return PHI_OPEN_ERR;

	struct alsar *al = ffmem_new(struct alsar);
	audio_in *a = &al->in;
	a->audio = &ffalsa;
	a->trk = t;

	if (0 != audio_in_open(a, t))
		goto fail;

	core->timer(&al->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
	return al;

fail:
	alsar_close(al, t);
	return PHI_OPEN_ERR;
}

static int alsar_read(struct alsar *al, phi_track *t)
{
	return audio_in_read(&al->in, t);
}

static const phi_filter phi_alsa_rec = {
	alsar_open, (void*)alsar_close, (void*)alsar_read,
	"alsa-rec"
};
