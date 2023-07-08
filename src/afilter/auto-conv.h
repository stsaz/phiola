/** phiola: auto audio conversion
2019, Simon Zolin */

#include <track.h>

struct autoconv {
	uint state;
	ffstr in;
};

static void* autoconv_open(phi_track *t)
{
	t->oaudio.format = t->audio.format;
	if (t->conf.stream_copy || !t->data_type || !ffsz_eq(t->data_type, "pcm"))
		return PHI_OPEN_SKIP;

	struct autoconv *c = ffmem_new(struct autoconv);
	return c;
}

static void autoconv_close(void *ctx, phi_track *t)
{
	ffmem_free(ctx);
}

static int autoconv_process(struct autoconv *c, phi_track *t)
{
	const struct phi_af *in;
	if (t->oaudio.format.format) {
		in = &t->oaudio.format;
	} else {
		in = &t->audio.format;
		t->oaudio.format = t->audio.format;
	}
	struct phi_af *out = &t->oaudio.conv_format;

	switch (c->state) {
	case 0:
		*out = *in;
		c->in = t->data_in;
		c->state = 1;
		return PHI_DATA;
	}

	if (!ffmem_cmp(in, out, sizeof(*in)))
		goto done; // no conversion is needed

	if (!core->track->filter(t, core->mod("afilter.conv"), 0))
		return PHI_ERR;

	t->aconv.in = *in;
	t->aconv.out = *out;

	t->oaudio.format = *out;

done:
	t->data_out = c->in;
	return PHI_DONE;
}

const phi_filter phi_autoconv = {
	autoconv_open, autoconv_close, (void*)autoconv_process,
	"auto-conv"
};
