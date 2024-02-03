/** phiola: Opus encode
2016, Simon Zolin */

#include <acodec/alib3-bridge/opus-enc-if.h>

struct opus_enc {
	uint state;
	struct phi_af fmt;
	ffopus_enc opus;
	uint64 npkt;
	uint64 endpos;
};

static void* opus_enc_create(phi_track *t)
{
	struct opus_enc *o = ffmem_new(struct opus_enc);
	if (!phi_metaif)
		phi_metaif = core->mod("format.meta");
	return o;
}

static void opus_enc_free(void *ctx, phi_track *t)
{
	struct opus_enc *o = ctx;
	ffopus_enc_close(&o->opus);
	ffmem_free(o);
}

static int opus_enc_addmeta(struct opus_enc *o, phi_track *t)
{
	uint i = 0;
	ffstr name, val;
	while (phi_metaif->list(&t->meta, &i, &name, &val, PHI_META_UNIQUE)) {
		if (ffstr_eqz(&name, "vendor"))
			continue;
		if (0 != ffopus_addtag(&o->opus, name.ptr, val.ptr, val.len))
			warnlog(t, "can't add tag: %S", &name);
	}

	return 0;
}

static int opus_enc_encode(void *ctx, phi_track *t)
{
	struct opus_enc *o = ctx;
	int r;
	enum { W_CONV, W_CREATE, W_DATA };

	switch (o->state) {
	case W_CONV: {
		o->opus.orig_sample_rate = t->oaudio.format.rate;
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

		o->opus.bandwidth = t->conf.opus.bandwidth;
		o->opus.mode = t->conf.opus.mode;
		o->opus.complexity = 0;
		o->opus.packet_dur = 40;
		uint br = (t->conf.opus.bitrate) ? t->conf.opus.bitrate : 192;
		if (0 != (r = ffopus_create(&o->opus, &o->fmt, br * 1000))) {
			errlog(t, "ffopus_create(): %s", ffopus_enc_errstr(&o->opus));
			return PHI_ERR;
		}

		o->opus.min_tagsize = 1000;
		opus_enc_addmeta(o, t);

		o->state = W_DATA;
		break;
	}

	if (t->chain_flags & PHI_FFIRST)
		o->opus.fin = 1;

	if (t->chain_flags & PHI_FFWD)
		o->opus.pcm = (void*)t->data_in.ptr,  o->opus.pcmlen = t->data_in.len;

	r = ffopus_encode(&o->opus);

	switch (r) {
	case FFOPUS_RMORE:
		return PHI_MORE;

	case FFOPUS_RDONE:

		t->audio.pos = ffopus_enc_pos(&o->opus);
		return PHI_DONE;

	case FFOPUS_RDATA:
		break;

	case FFOPUS_RERR:
		errlog(t, "ffopus_encode(): %s", ffopus_enc_errstr(&o->opus));
		return PHI_ERR;
	}

	t->audio.pos = o->endpos;
	o->endpos = ffopus_enc_pos(&o->opus);
	o->npkt++;
	dbglog(t, "encoded %L samples into %L bytes @%U [%U]"
		, (t->data_in.len - o->opus.pcmlen) / pcm_size1(&o->fmt), o->opus.data.len
		, t->audio.pos, o->endpos);
	ffstr_set(&t->data_out, o->opus.data.ptr, o->opus.data.len);
	return PHI_DATA;
}

static const phi_filter phi_opus_enc = {
	opus_enc_create, opus_enc_free, opus_enc_encode,
	"opus-encode"
};
