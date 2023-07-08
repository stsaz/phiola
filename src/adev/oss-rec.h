/** phiola: OSS
2017, Simon Zolin */

typedef struct oss_in {
	audio_in in;
	phi_timer tmr;
} oss_in;

static void* oss_in_open(phi_track *d)
{
	if (0 != mod_init(d->trk))
		return PHI_OPEN_ERR;

	oss_in *pi = ffmem_new(oss_in);
	audio_in *a = &pi->in;
	a->audio = &ffoss;
	a->trk = d->trk;

	if (0 != audio_in_open(a, d))
		goto fail;

	core->timer(&pi->tmr, a->buffer_length_msec / 2, audio_oncapt, a);
	return pi;

fail:
	oss_in_close(pi);
	return PHI_OPEN_ERR;
}

static void oss_in_close(void *ctx, phi_track *t)
{
	oss_in *pi = ctx;
	core->timer(&pi->tmr, 0, NULL, NULL);
	audio_in_close(&pi->in);
	ffmem_free(pi);
}

static int oss_in_read(void *ctx, phi_track *d)
{
	oss_in *pi = ctx;
	return audio_in_read(&pi->in, d);
}

static const phi_filter phi_oss_rec = {
	oss_in_open, oss_in_close, oss_in_read,
	"oss-rec"
};