/** phiola: Opus input
2016, Simon Zolin */

#include <avpack/base/opus.h>
#include <opus/opus-phi.h>

struct opus_dec {
	uint state;
	uint sample_size;
	uint64 pos;
	uint channels;
	uint preskip;
	uint reset_decoder;
	opus_ctx *dec;
	ffvec obuf;
};

static void* opus_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct opus_dec *o = phi_track_allocT(t, struct opus_dec);
	return o;
}

static void opus_close(void *ctx, phi_track *t)
{
	struct opus_dec *o = ctx;
	ffvec_free(&o->obuf);
	opus_decode_free(o->dec);
	phi_track_free(t, o);
}

static int opus_dec_init(struct opus_dec *o, phi_track *t, ffstr in)
{
	if (!opus_hdr_read(in.ptr, in.len, &o->channels, &o->preskip)) {
		errlog(t, "bad Opus header");
		return PHI_ERR;
	}

	opus_conf conf = {
		.channels = o->channels,
	};
	int r;
	if ((r = opus_decode_init(&o->dec, &conf))) {
		errlog(t, "opus_decode_init(): %s", opus_errstr(r));
		return PHI_ERR;
	}

	t->audio.format.format = PHI_PCM_FLOAT32;
	t->audio.format.interleaved = 1;
	t->audio.start_delay = ffmin(o->preskip, 4800);

	o->sample_size = phi_af_size(&t->audio.format);
	t->data_type = "pcm";
	ffvec_alloc(&o->obuf, OPUS_BUFLEN(48000) * o->channels * sizeof(float), 1);
	return 0;
}

static int opus_in_decode(void *ctx, phi_track *t)
{
	enum { R_INIT, R_TAGS, R_DATA1, R_DATA };
	struct opus_dec *o = ctx;
	int r;
	ffstr in = {};

	if (t->chain_flags & PHI_FFWD) {
		in = t->data_in;
	}

	for (;;) {
		switch (o->state) {
		case R_INIT:
			if (in.len == 0)
				return PHI_MORE;
			if (opus_dec_init(o, t, in))
				return PHI_ERR;
			o->state = R_TAGS;
			return PHI_MORE;

		case R_TAGS:
			if (in.len == 0)
				return PHI_MORE;
			o->state = R_DATA1;
			if (opus_tags_read(in.ptr, in.len))
				return PHI_MORE;
			// fallthrough

		case R_DATA1:
			if (t->audio.total != ~0ULL) {
				t->audio.total -= o->preskip;
				t->audio.end_padding = 1;
			}

			if (t->conf.info_only)
				return PHI_LASTOUT;

			o->state = R_DATA;
			break;
		}

		if (t->audio.ogg_reset) {
			t->audio.ogg_reset = 0;
			ffvec_free(&o->obuf);
			if (o->dec)
				opus_decode_free(o->dec);
			ffmem_zero_obj(o);
			continue;
		}

		break;
	}

	if (t->audio.seek_req) {
		// A new seek request is received.  Pass control to UI module.
		o->reset_decoder = 1;
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
	}

	if (in.len == 0)
		goto more;

	if (t->chain_flags & PHI_FFWD) {
		if (o->reset_decoder) {
			o->reset_decoder = 0;
			opus_decode_reset(o->dec);
		}

		if (t->audio.pos != ~0ULL)
			o->pos = t->audio.pos;
	}

	r = opus_decode_f(o->dec, in.ptr, in.len, (float*)o->obuf.ptr);
	if (r < 0) {
		warnlog(t, "opus_decode_f(): %s", opus_errstr(r));
		goto more;
	}

	ffstr_set(&t->data_out, o->obuf.ptr, r * o->channels * sizeof(float));
	t->audio.pos = o->pos;
	o->pos += r;
	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / o->sample_size, t->audio.pos);
	return PHI_DATA;

more:
	return !(t->chain_flags & PHI_FFIRST) ? PHI_MORE : PHI_DONE;
}

static const phi_filter phi_opus_dec = {
	opus_open, opus_close, opus_in_decode,
	"opus-decode"
};
