/** phiola: auto audio conversion
2019, Simon Zolin */

#include <track.h>
#include <util/aformat.h>

struct autoconv {
	uint state;
	ffstr in;
};

static void* autoconv_open(phi_track *t)
{
	t->oaudio.format = t->audio.format;
	if (t->conf.stream_copy || !t->data_type || !ffsz_eq(t->data_type, "pcm"))
		return PHI_OPEN_SKIP;

	struct autoconv *c = phi_track_allocT(t, struct autoconv);
	return c;
}

static void autoconv_close(void *ctx, phi_track *t)
{
	phi_track_free(t, ctx);
}

static int autoconv_process(struct autoconv *c, phi_track *t)
{
	const struct phi_af *iaf = &t->audio.format;
	struct phi_af *oaf = &t->oaudio.format;

	if (c->state == 0) {
		phi_af_update(oaf, &t->conf.oaudio.format); // apply settings from user
		t->oaudio.conv_format.interleaved = oaf->interleaved;
		char buf[100];
		dbglog(t, "request audio format: %s", phi_af_print(oaf, buf, 100));
		c->in = t->data_in;
		c->state = 1;
		return PHI_DATA;
	}

	phi_af_update(oaf, &t->oaudio.conv_format); // apply settings from encoder
	oaf->interleaved = t->oaudio.conv_format.interleaved;

	if (!ffmem_cmp(iaf, oaf, sizeof(*iaf)))
		goto done; // no conversion is needed

	t->aconv.in = *iaf;
	t->aconv.out = *oaf;
	if (!core->track->filter(t, core->mod("afilter.conv"), 0))
		return PHI_ERR;

done:
	t->data_out = c->in;
	return PHI_DONE;
}

const phi_filter phi_autoconv = {
	autoconv_open, autoconv_close, (void*)autoconv_process,
	"auto-conv"
};
