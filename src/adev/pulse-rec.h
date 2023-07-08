/** phiola: record via PulseAudio
2017, Simon Zolin */

struct pulsr {
	audio_in in;
	phi_timer tmr;
};

static void pulsr_close(struct pulsr *p, phi_track *t)
{
	core->timer(&p->tmr, 0, NULL, NULL);
	audio_in_close(&p->in);
	ffmem_free(p);
}

static void* pulsr_open(phi_track *t)
{
	if (0 != pulse_init(t))
		return PHI_OPEN_ERR;

	struct pulsr *p = ffmem_new(struct pulsr);
	audio_in *a = &p->in;
	a->audio = &ffpulse;
	a->trk = t;

	int r;
	int retry = 0;
	while (0 != (r = audio_in_open(a, t))) {
		if (!retry && r == FFAUDIO_ECONNECTION) {
			retry = 1;
			dbglog(t, "lost connection to audio server, reconnecting...");
			audio_in_close(&p->in);

			a->audio->uninit();
			mod->init_ok = 0;

			if (0 != pulse_init(t)) {
				goto fail;
			}
			continue;
		}

		goto fail;
	}

	core->timer(&p->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
	return p;

fail:
	pulsr_close(p, t);
	return PHI_OPEN_ERR;
}

static int pulsr_read(struct pulsr *p, phi_track *t)
{
	return audio_in_read(&p->in, t);
}

static const struct phi_filter phi_pulse_rec = {
	pulsr_open, (void*)pulsr_close, (void*)pulsr_read,
	"pulse-rec"
};
