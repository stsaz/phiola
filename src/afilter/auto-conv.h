/** phiola: auto audio conversion
2019, Simon Zolin */

#include <track.h>
#include <util/aformat.h>

struct autoconv {
	uint state;
	ffstr in;
	struct phi_af iaf, oaf;
};

static void* autoconv_open(phi_track *t)
{
	if (!t->oaudio.format.format)
		t->oaudio.format = t->audio.format;
	if (t->conf.stream_copy || !t->data_type || !ffsz_eq(t->data_type, "pcm"))
		return PHI_OPEN_SKIP;

	struct autoconv *c = phi_track_allocT(t, struct autoconv);
	c->iaf = t->oaudio.format;
	return c;
}

static void autoconv_close(void *ctx, phi_track *t)
{
	phi_track_free(t, ctx);
}

static int autoconv_process(struct autoconv *c, phi_track *t)
{
	struct phi_af *oaf = &t->oaudio.format;

	if (c->state == 0) {
		phi_af_update(oaf, &t->conf.oaudio.format); // apply settings from user
		if (t->conf.oaudio.format.interleaved)
			oaf->interleaved = 1;
		t->oaudio.conv_format.interleaved = oaf->interleaved;
		char buf[100];
		dbglog(t, "request audio format: %s", phi_af_print(oaf, buf, 100));
		c->in = t->data_in;
		c->state = 1;
		return PHI_DATA;
	}

	phi_af_update(oaf, &t->oaudio.conv_format); // apply settings from encoder
	oaf->interleaved = t->oaudio.conv_format.interleaved;

	if (!ffmem_cmp(&c->iaf, oaf, sizeof(c->iaf)))
		goto done; // no conversion is needed

	t->aconv.in = c->iaf;
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


static void* autoconv_f_open(phi_track *t)
{
	PHI_ASSERT(t->data_type && ffsz_eq(t->data_type, "pcm"));
	struct autoconv *c = phi_track_allocT(t, struct autoconv);
	return c;
}

static void autoconv_f_close(void *ctx, phi_track *t)
{
	phi_track_free(t, ctx);
}

static int autoconv_f_process(struct autoconv *c, phi_track *t)
{
	if (c->state == 0) {
		c->iaf = t->audio.format;
		c->oaf = c->iaf;
		c->oaf.format = PHI_PCM_FLOAT64;
		c->oaf.interleaved = 1;
		FF_ASSERT(!t->oaudio.format.format);
		t->oaudio.format = c->oaf;
		char buf[100];
		dbglog(t, "filtering audio format: %s", phi_af_print(&c->oaf, buf, 100));
		c->in = t->data_in;
		c->state = 1;
		return PHI_DATA;
	}

	if (ffmem_cmp(&c->iaf, &c->oaf, sizeof(c->iaf))) {
		t->aconv.in = c->iaf;
		t->aconv.out = c->oaf;
		if (!core->track->filter(t, core->mod("afilter.conv"), 0))
			return PHI_ERR;
	}

	t->data_out = c->in;
	return PHI_DONE;
}

const phi_filter phi_autoconv_f = {
	autoconv_f_open, autoconv_f_close, (void*)autoconv_f_process,
	"auto-conv-f"
};
