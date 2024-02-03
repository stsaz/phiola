/** Vorbis encoder interface
2016, Simon Zolin */

#pragma once
#include <afilter/pcm.h>
#include <ffbase/string.h>
#include <avpack/vorbistag.h>
#include <vorbis/vorbis-phi.h>

typedef struct ffvorbis_enc {
	uint state;
	vorbis_ctx *vctx;
	uint channels;
	uint sample_rate;
	int err;
	ffstr pkt_hdr;
	ffstr pkt_book;

	vorbistagwrite vtag;
	uint min_tagsize;

	ffstr data;
	ffvec buf;

	size_t pcmlen;
	const float **pcm; //non-interleaved
	uint64 granulepos;

	uint fin :1;
} ffvorbis_enc;

#define ffvorbis_enc_errstr(v)  _ffvorbis_errstr((v)->err)

/** Add vorbis tag. */
static inline int ffvorbis_addtag(ffvorbis_enc *v, const char *name, const char *val, ffsize val_len)
{
	ffstr nm = FFSTR_INITZ(name),  d = FFSTR_INITN(val, val_len);
	return vorbistagwrite_add_name(&v->vtag, nm, d);
}

#define ffvorbis_enc_pos(v)  ((v)->granulepos)

/**
quality_x10: q*10
*/
int ffvorbis_create(ffvorbis_enc *v, const struct phi_af *fmt, int quality_x10)
{
	int r;

	if (fmt->format != PHI_PCM_FLOAT32)
		return ERR(v, FFVORBIS_EFMT);

	vorbis_encode_params params = {0};
	params.channels = fmt->channels;
	params.rate = fmt->rate;
	params.quality = (float)quality_x10 / 100;
	ogg_packet pkt[2];
	if (0 != (r = vorbis_encode_create(&v->vctx, &params, &pkt[0], &pkt[1])))
		return ERR(v, r);

	const char *svendor = vorbis_vendor();
	ffstr vendor = FFSTR_INITZ(svendor);
	vorbistagwrite_add(&v->vtag, MMTAG_VENDOR, vendor);

	v->channels = fmt->channels;
	v->sample_rate = fmt->rate;
	ffstr_set(&v->pkt_hdr, pkt[0].packet, pkt[0].bytes);
	ffstr_set(&v->pkt_book, pkt[1].packet, pkt[1].bytes);
	v->min_tagsize = 1000;
	return 0;
}

void ffvorbis_enc_close(ffvorbis_enc *v)
{
	ffvec_free(&v->buf);
	vorbistagwrite_destroy(&v->vtag);
	if (v->vctx) {
		vorbis_encode_free(v->vctx);
		v->vctx = NULL;
	}
}

/** Get complete packet with Vorbis comments and padding. */
static int _ffvorbis_tags(ffvorbis_enc *v, ffstr *pkt)
{
	ffstr tags = vorbistagwrite_fin(&v->vtag);
	uint npadding = (tags.len < v->min_tagsize) ? v->min_tagsize - tags.len : 0;

	v->buf.len = 0;
	if (NULL == ffvec_realloc(&v->buf, sizeof(struct vorbis_hdr) + tags.len + 1 + npadding, 1)) // hdr, tags, "framing bit", padding
		return FFVORBIS_ESYS;

	char *d = v->buf.ptr;
	ffmem_copy(&d[sizeof(struct vorbis_hdr)], tags.ptr, tags.len);
	ffsize i = vorb_comm_write(d, tags.len);

	ffmem_zero(&d[i], npadding);
	v->buf.len = i + npadding;

	ffstr_setstr(pkt, &v->buf);
	return 0;
}

/**
Return enum FFVORBIS_R. */
int ffvorbis_encode(ffvorbis_enc *v)
{
	ogg_packet pkt;
	int r;
	enum { W_HDR, W_COMM, W_BOOK, W_DATA };

	switch (v->state) {
	case W_HDR:
		ffstr_set2(&v->data, &v->pkt_hdr);
		v->state = W_COMM;
		return FFVORBIS_RDATA;

	case W_COMM:
		if (0 != (r = _ffvorbis_tags(v, &v->data)))
			return ERR(v, r);

		v->state = W_BOOK;
		return FFVORBIS_RDATA;

	case W_BOOK:
		ffstr_set2(&v->data, &v->pkt_book);
		v->state = W_DATA;
		return FFVORBIS_RDATA;
	}

	int n = (uint)(v->pcmlen / (sizeof(float) * v->channels));
	v->pcmlen = 0;

	for (;;) {
		r = vorbis_encode(v->vctx, v->pcm, n, &pkt);
		if (r < 0) {
			v->err = r;
			return FFVORBIS_RERR;
		} else if (r == 0) {
			if (v->fin) {
				n = -1;
				continue;
			}
			return FFVORBIS_RMORE;
		}
		break;
	}

	v->granulepos = pkt.granulepos;
	ffstr_set(&v->data, pkt.packet, pkt.bytes);
	return (pkt.e_o_s) ? FFVORBIS_RDONE : FFVORBIS_RDATA;
}
