/** phiola: record via AAudio
2023, Simon Zolin */

struct aai {
	audio_in in;
};

static void aai_close(void *ctx, phi_track *t)
{
	struct aai *c = ctx;
	audio_in_close(&c->in);
	ffmem_free(c);
}

static void* aai_open(phi_track *t)
{
	struct aai *c = ffmem_new(struct aai);
	audio_in *a = &c->in;
	a->audio = &ffaaudio;
	a->trk = t;
	a->aflags |= FFAUDIO_O_UNSYNC_NOTIFY;
	a->aflags |= (t->conf.iaudio.power_save) ? FFAUDIO_O_POWER_SAVE : 0;
	a->aflags |= (t->conf.iaudio.exclusive) ? FFAUDIO_O_EXCLUSIVE : 0;
	a->recv_events = 1;

	if (0 != audio_in_open(a, t))
		goto err;

	return c;

err:
	aai_close(c, t);
	return PHI_OPEN_ERR;
}

static int aai_read(struct aai *c, phi_track *t)
{
	return audio_in_read(&c->in, t);
}

static const phi_filter phi_aaudio_rec = {
	aai_open, (void*)aai_close, (void*)aai_read,
	"aaudio-rec"
};
