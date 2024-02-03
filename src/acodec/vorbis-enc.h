/** phiola: Vorbis encode
2016, Simon Zolin */

#include <acodec/alib3-bridge/vorbis-enc-if.h>

struct vorbis_enc {
	uint state;
	struct phi_af fmt;
	ffstr in;
	ffvorbis_enc vorbis;
	uint64 npkt;
	ffuint64 endpos;
};

static void* vorbis_out_create(phi_track *t)
{
	struct vorbis_enc *v = ffmem_new(struct vorbis_enc);
	if (!phi_metaif)
		phi_metaif = core->mod("format.meta");
	return v;
}

static void vorbis_out_free(void *ctx, phi_track *t)
{
	struct vorbis_enc *v = ctx;
	ffvorbis_enc_close(&v->vorbis);
	ffmem_free(v);
}

static int vorbis_out_addmeta(struct vorbis_enc *v, phi_track *t)
{
	uint i = 0;
	ffstr name, val;
	while (phi_metaif->list(&t->meta, &i, &name, &val, PHI_META_UNIQUE)) {
		if (ffstr_eqz(&name, "vendor")
			|| ffstr_eqz(&name, "picture"))
			continue;
		if (0 != ffvorbis_addtag(&v->vorbis, name.ptr, val.ptr, val.len))
			warnlog(t, "can't add tag: %S", &name);
	}

	return 0;
}

static int vorbis_out_encode(void *ctx, phi_track *t)
{
	struct vorbis_enc *v = ctx;
	int r;
	enum { W_CONV, W_CREATE, W_DATA, W_FIN };

	if (t->chain_flags & PHI_FFWD) {
		v->in = t->data_in;
	}

	switch (v->state) {
	case W_CONV:
		t->oaudio.conv_format.format = PHI_PCM_FLOAT32;
		t->oaudio.conv_format.interleaved = 0;
		v->state = W_CREATE;
		return PHI_MORE;

	case W_CREATE:
		if (t->oaudio.format.format != PHI_PCM_FLOAT32
			|| t->oaudio.format.interleaved) {
			errlog(t, "input format must be float32 non-interleaved");
			return PHI_ERR;
		}
		t->data_type = "Vorbis";

		v->fmt = t->oaudio.format;
		uint q_x10 = (t->conf.vorbis.quality) ? (t->conf.vorbis.quality - 10) : 5*10;
		if (0 != (r = ffvorbis_create(&v->vorbis, &v->fmt, q_x10))) {
			errlog(t, "ffvorbis_create(): %s", ffvorbis_enc_errstr(&v->vorbis));
			return PHI_ERR;
		}

		v->vorbis.min_tagsize = 1000;
		vorbis_out_addmeta(v, t);

		v->state = W_DATA;
		break;

	case W_FIN:
		t->audio.pos = v->endpos;
		return PHI_DONE;
	}

	if (t->chain_flags & PHI_FFIRST)
		v->vorbis.fin = 1;

	v->vorbis.pcm = (const float**)v->in.ptr,  v->vorbis.pcmlen = v->in.len;
	r = ffvorbis_encode(&v->vorbis);

	switch (r) {
	case FFVORBIS_RMORE:
		return PHI_MORE;

	case FFVORBIS_RDONE:
		v->state = W_FIN;
		break;
	case FFVORBIS_RDATA:
		break;

	case FFVORBIS_RERR:
		errlog(t, "ffvorbis_encode(): %s", ffvorbis_enc_errstr(&v->vorbis));
		return PHI_ERR;
	}

	v->npkt++;
	t->audio.pos = v->endpos;
	v->endpos = ffvorbis_enc_pos(&v->vorbis);
	dbglog(t, "encoded %L samples into %L bytes @%U [%U]"
		, (v->in.len - v->vorbis.pcmlen) / pcm_size1(&v->fmt), v->vorbis.data.len
		, t->audio.pos, v->endpos);
	ffstr_set(&v->in, v->vorbis.pcm, v->vorbis.pcmlen);
	ffstr_set(&t->data_out, v->vorbis.data.ptr, v->vorbis.data.len);
	return PHI_DATA;
}

static const phi_filter phi_vorbis_enc = {
	vorbis_out_create, vorbis_out_free, vorbis_out_encode,
	"vorbis-encode"
};
