/** phiola: Vorbis encode
2016, Simon Zolin */

#include <avpack/vorbistag.h>
#include <avpack/base/vorbis.h>
#include <vorbis/vorbis-phi.h>

struct vorbis_enc {
	uint state;
	struct phi_af fmt;
	vorbis_ctx *vctx;
	uint64 endpos, granulepos;
	ffstr in;
	ffstr tags;
	ffstr pkt_book;

	size_t pcmlen;
	const float **pcm; //non-interleaved
};

static void* vorbis_enc_create(phi_track *t)
{
	struct vorbis_enc *v = phi_track_allocT(t, struct vorbis_enc);
	return v;
}

static void vorbis_enc_free(void *ctx, phi_track *t)
{
	struct vorbis_enc *v = ctx;
	ffstr_free(&v->tags);
	if (v->vctx)
		vorbis_encode_free(v->vctx);
	phi_track_free(t, v);
}

static int vorbis_enc_init(struct vorbis_enc *v, phi_track *t, ffstr *out)
{
	t->data_type = PHI_AC_VORBIS;
	v->fmt = t->oaudio.format;
	uint q_x10 = (t->conf.vorbis.quality) ? (t->conf.vorbis.quality - 10) : 5*10;

	vorbis_encode_params params = {
		.quality = (float)q_x10 / 100,
		.channels = v->fmt.channels,
		.rate = v->fmt.rate,
	};
	ogg_packet pkt[2];
	int r;
	if ((r = vorbis_encode_create(&v->vctx, &params, &pkt[0], &pkt[1]))) {
		errlog(t, "vorbis_encode_create(): %s", vorbis_errstr(r));
		return PHI_ERR;
	}

	ffstr_set(&v->pkt_book, pkt[1].packet, pkt[1].bytes);

	ffstr_set(out, pkt[0].packet, pkt[0].bytes);
	return 0;
}

static void vorbis_enc_tags(struct vorbis_enc *v, phi_track *t, ffstr *out)
{
	vorbistagwrite vtag = {
		.left_zone = 7
	};
	vorbistagwrite_create(&vtag);
	vorbistagwrite_add(&vtag, MMTAG_VENDOR, FFSTR_Z(vorbis_vendor()));

	uint i = 0;
	ffstr name, val;
	while (core->metaif->list(&t->meta, &i, &name, &val, PHI_META_UNIQUE)) {
		if (ffstr_eqz(&name, "vendor")
			|| ffstr_eqz(&name, "picture"))
			continue;
		vorbistagwrite_add_name(&vtag, name, val);
	}

	uint tags_len = vorbistagwrite_fin(&vtag).len;
	int padding = 1000 - tags_len;
	if (padding < 0)
		padding = 0;
	ffvec_grow(&vtag.out, padding + 1, 1);
	ffstr vt = *(ffstr*)&vtag.out;
	vt.len = vorbis_tags_write(vt.ptr, vtag.out.cap, tags_len);

	ffmem_zero(vt.ptr + vt.len, padding);
	vt.len += padding;

	// vorbistagwrite_destroy(&vtag);
	v->tags = vt;
	*out = vt;
}

static int vorbis_enc_encode(void *ctx, phi_track *t)
{
	struct vorbis_enc *v = ctx;
	int r;
	enum { W_CONV, W_CREATE, W_TAGS, W_BOOK, W_DATA, W_FIN };

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

		if (vorbis_enc_init(v, t, &t->data_out))
			return PHI_ERR;
		v->state = W_TAGS;
		return PHI_DATA;

	case W_TAGS:
		vorbis_enc_tags(v, t, &t->data_out);
		v->state = W_BOOK;
		return PHI_DATA;

	case W_BOOK:
		t->data_out = v->pkt_book;
		v->state = W_DATA;
		return PHI_DATA;

	case W_FIN:
		t->audio.pos = v->endpos;
		return PHI_DONE;
	}

	v->pcm = (const float**)v->in.ptr,  v->pcmlen = v->in.len;
	int n = (uint)(v->pcmlen / (sizeof(float) * v->fmt.channels));
	v->pcmlen = 0;

	ogg_packet pkt;
	for (;;) {
		r = vorbis_encode(v->vctx, v->pcm, n, &pkt);
		if (r < 0) {
			errlog(t, "vorbis_encode(): %s", vorbis_errstr(r));
			return PHI_ERR;
		} else if (r == 0) {
			if (t->chain_flags & PHI_FFIRST) {
				n = -1;
				continue;
			}
			return PHI_MORE;
		}
		break;
	}

	ffstr_set(&t->data_out, pkt.packet, pkt.bytes);
	t->audio.pos = v->endpos;
	v->granulepos = (pkt.granulepos) ? pkt.granulepos : 1;
	v->endpos = v->granulepos;
	dbglog(t, "encoded %L samples into %L bytes @%U [%U]"
		, (v->in.len - v->pcmlen) / phi_af_size(&v->fmt), t->data_out.len
		, t->audio.pos, v->endpos);
	ffstr_set(&v->in, v->pcm, v->pcmlen);
	if (pkt.e_o_s)
		v->state = W_FIN;
	return PHI_DATA;
}

static const phi_filter phi_vorbis_enc = {
	vorbis_enc_create, vorbis_enc_free, vorbis_enc_encode,
	"vorbis-encode"
};
