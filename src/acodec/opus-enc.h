/** phiola: Opus encode
2016, Simon Zolin */

#include <avpack/vorbistag.h>
#include <avpack/base/opus.h>
#include <ffbase/vector.h>
#include <opus/opus-phi.h>

struct opus_enc {
	uint state;
	uint frame_samples;
	uint frame_size;
	uint orig_sample_rate;
	struct phi_af fmt;
	ffstr tags;

	opus_ctx *enc;
	uint64 endpos;
	ffstr in;
	ffvec ibuf, obuf;
};

static void* opus_enc_create(phi_track *t)
{
	struct opus_enc *o = phi_track_allocT(t, struct opus_enc);
	return o;
}

static void opus_enc_free(void *ctx, phi_track *t)
{
	struct opus_enc *o = ctx;
	ffstr_free(&o->tags);
	ffvec_free(&o->obuf);
	ffvec_free(&o->ibuf);
	opus_encode_free(o->enc);
	phi_track_free(t, o);
}

static int opus_enc_init(struct opus_enc *o, phi_track *t, ffstr *out)
{
	uint br = (t->conf.opus.bitrate) ? t->conf.opus.bitrate : 192;
	opus_encode_conf conf = {
		.channels = o->fmt.channels,
		.sample_rate = o->fmt.rate,
		.bitrate = br * 1000,
		.complexity = 10 + 1,
		.bandwidth = t->conf.opus.bandwidth,
		.application = t->conf.opus.mode,
	};
	int r;
	if ((r = opus_encode_create(&o->enc, &conf))) {
		errlog(t, "opus_encode_create(): %s", opus_errstr(r));
		return PHI_ERR;
	}
	o->frame_samples = msec_to_samples(40, 48000);
	o->frame_size = o->frame_samples * sizeof(float) * o->fmt.channels;

	uint padding = conf.preskip * sizeof(float) * o->fmt.channels;
	ffvec_alloc(&o->ibuf, ffmax(padding, o->frame_size), 1);
	ffmem_zero(o->ibuf.ptr, padding);
	o->ibuf.len = padding;

	ffvec_alloc(&o->obuf, OPUS_MAX_PKT, 1);
	r = opus_hdr_write(o->obuf.ptr, o->obuf.cap, o->fmt.channels, o->orig_sample_rate, conf.preskip);
	ffstr_set(out, o->obuf.ptr, r);
	return 0;
}

static void opus_enc_tags(struct opus_enc *o, phi_track *t, ffstr *out)
{
	vorbistagwrite vtag = {
		.left_zone = 8
	};
	vorbistagwrite_create(&vtag);
	vorbistagwrite_add(&vtag, MMTAG_VENDOR, FFSTR_Z(opus_vendor()));

	uint i = 0;
	ffstr name, val;
	while (core->metaif->list(&t->meta, &i, &name, &val, PHI_META_UNIQUE)) {
		if (ffstr_eqz(&name, "vendor"))
			continue;
		vorbistagwrite_add_name(&vtag, name, val);
	}

	uint tags_len = vorbistagwrite_fin(&vtag).len;
	int padding = 1000 - tags_len;
	if (padding < 0)
		padding = 0;
	ffvec_grow(&vtag.out, padding, 1);
	ffstr vt = *(ffstr*)&vtag.out;
	vt.len = opus_tags_write(vt.ptr, vtag.out.cap, tags_len);

	ffmem_zero(vt.ptr + vt.len, padding);
	vt.len += padding;

	// vorbistagwrite_destroy(&vtag);
	o->tags = vt;
	*out = vt;
}

static int opus_enc_encode(void *ctx, phi_track *t)
{
	struct opus_enc *o = ctx;
	int r;
	uint in_len;
	ffstr d = {};
	enum { W_CONV, W_CREATE, W_TAGS, W_DATA, W_DONE };

	if (t->chain_flags & PHI_FFWD)
		o->in = t->data_in;

	switch (o->state) {
	case W_CONV: {
		o->orig_sample_rate = t->oaudio.format.rate;
		struct phi_af f = {
			.format = PHI_PCM_FLOAT32,
			.rate = 48000,
			.channels = t->oaudio.format.channels,
			.interleaved = 1,
		};
		t->oaudio.conv_format = f;
		o->state = W_CREATE;
		return PHI_MORE;
	}

	case W_CREATE:
		o->fmt = t->oaudio.format;
		if (o->fmt.format != PHI_PCM_FLOAT32
			|| o->fmt.rate != 48000
			|| !t->oaudio.format.interleaved) {
			errlog(t, "input format must be float32 48kHz interleaved");
			return PHI_ERR;
		}
		t->data_type = "Opus";

		if (opus_enc_init(o, t, &t->data_out))
			return PHI_ERR;
		o->state = W_TAGS;
		return PHI_DATA;

	case W_TAGS:
		opus_enc_tags(o, t, &t->data_out);
		o->state = W_DATA;
		return PHI_DATA;

	case W_DATA:
		break;

	case W_DONE:
		t->audio.pos = o->endpos;
		return PHI_DONE;
	}

	in_len = o->in.len;
	r = ffstr_gather((ffstr*)&o->ibuf, &o->ibuf.cap, o->in.ptr, o->in.len, o->frame_size, &d);
	ffstr_shift(&o->in, r);
	uint samples = o->frame_samples;
	if (d.len < o->frame_size) {
		if (!(t->chain_flags & PHI_FFIRST))
			return PHI_MORE;
		uint padding = o->frame_size - o->ibuf.len;
		samples = o->ibuf.len / (sizeof(float) * o->fmt.channels);
		ffmem_zero(o->ibuf.ptr + o->ibuf.len, padding);
		d.ptr = o->ibuf.ptr;
		o->state = W_DONE;
	}
	o->ibuf.len = 0;

	r = opus_encode_f(o->enc, (float*)d.ptr, o->frame_samples, o->obuf.ptr);
	if (r < 0) {
		errlog(t, "opus_encode_f(): %s", opus_errstr(r));
		return PHI_ERR;
	}
	ffstr_set(&t->data_out, o->obuf.ptr, r);

	t->audio.pos = o->endpos;
	o->endpos += samples;
	dbglog(t, "encoded %L samples into %L bytes @%U [%U]"
		, (in_len - o->in.len) / phi_af_size(&o->fmt), t->data_out.len
		, t->audio.pos, o->endpos);
	return PHI_DATA;
}

static const phi_filter phi_opus_enc = {
	opus_enc_create, opus_enc_free, opus_enc_encode,
	"opus-encode"
};
