/** Vorbis.
2016, Simon Zolin */

/*
OGG(VORB_INFO)  OGG(VORB_COMMENTS VORB_CODEBOOK)  OGG(PKT1 PKT2...)...
*/

#pragma once
#include <afilter/pcm.h>
#include <ffbase/string.h>
#include <avpack/vorbistag.h>
#include <vorbis/vorbis-ff.h>

enum {
	FFVORBIS_EFMT = 1,
	FFVORBIS_EPKT,
	FFVORBIS_ETAG,

	FFVORBIS_ESYS,
};


enum FFVORBIS_R {
	FFVORBIS_RWARN = -2,
	FFVORBIS_RERR = -1,
	FFVORBIS_RHDR, //audio info is parsed
	FFVORBIS_RHDRFIN, //header is finished
	FFVORBIS_RDATA, //PCM data is returned
	FFVORBIS_RMORE,
	FFVORBIS_RDONE,
};

typedef struct ffvorbis {
	uint state;
	int err;
	vorbis_ctx *vctx;
	struct {
		uint channels;
		uint rate;
		uint bitrate_nominal;
	} info;
	uint pktno;
	uint pkt_samples;
	uint64 total_samples;
	uint64 cursample;
	uint64 seek_sample;

	size_t pcmlen;
	const float **pcm; //non-interleaved
	const float* pcm_arr[8];
	uint fin :1;
} ffvorbis;

#define ffvorbis_errstr(v)  _ffvorbis_errstr((v)->err)

/** Get bps. */
#define ffvorbis_bitrate(v)  ((v)->info.bitrate_nominal)

#define ffvorbis_rate(v)  ((v)->info.rate)
#define ffvorbis_channels(v)  ((v)->info.channels)

/** Get an absolute sample number. */
#define ffvorbis_cursample(v)  ((v)->cursample - (v)->pkt_samples)

enum VORBIS_HDR_T {
	T_INFO = 1,
	T_COMMENT = 3,
};

struct vorbis_hdr {
	ffbyte type; //enum VORBIS_HDR_T
	char vorbis[6]; //"vorbis"
};

#define VORB_STR  "vorbis"

struct vorbis_info {
	ffbyte ver[4]; //0
	ffbyte channels;
	ffbyte rate[4];
	ffbyte br_max[4];
	ffbyte br_nominal[4];
	ffbyte br_min[4];
	ffbyte blocksize;
	ffbyte framing_bit; //1
};

/** Parse Vorbis-info packet. */
static int vorb_info(const char *d, size_t len, uint *channels, uint *rate, uint *br_nominal);

/**
Return pointer to the beginning of Vorbis comments data;  NULL if not Vorbis comments header. */
static void* vorb_comm(const char *d, size_t len, size_t *vorbtag_len);

/** Prepare OGG packet for Vorbis comments.
@d: buffer for the whole packet, must have 1 ffbyte of free space at the end
Return packet length. */
static uint vorb_comm_write(char *d, size_t vorbtag_len);


int vorb_info(const char *d, size_t len, uint *channels, uint *rate, uint *br_nominal)
{
	const struct vorbis_hdr *h = (void*)d;
	if (len < sizeof(struct vorbis_hdr) + sizeof(struct vorbis_info)
		|| !(h->type == T_INFO && !ffmem_cmp(h->vorbis, VORB_STR, FFS_LEN(VORB_STR))))
		return -1;

	const struct vorbis_info *vi = (void*)(d + sizeof(struct vorbis_hdr));
	if (0 != ffint_le_cpu32_ptr(vi->ver)
		|| 0 == (*channels = vi->channels)
		|| 0 == (*rate = ffint_le_cpu32_ptr(vi->rate))
		|| vi->framing_bit != 1)
		return -1;

	*br_nominal = ffint_le_cpu32_ptr(vi->br_nominal);
	return 0;
}

void* vorb_comm(const char *d, size_t len, size_t *vorbtag_len)
{
	const struct vorbis_hdr *h = (void*)d;

	if (len < (uint)sizeof(struct vorbis_hdr)
		|| !(h->type == T_COMMENT && !ffmem_cmp(h->vorbis, VORB_STR, FFS_LEN(VORB_STR))))
		return NULL;

	return (char*)d + sizeof(struct vorbis_hdr);
}

uint vorb_comm_write(char *d, size_t vorbtag_len)
{
	struct vorbis_hdr *h = (void*)d;
	h->type = T_COMMENT;
	ffmem_copy(h->vorbis, VORB_STR, FFS_LEN(VORB_STR));
	d[sizeof(struct vorbis_hdr) + vorbtag_len] = 1; //set framing bit
	return sizeof(struct vorbis_hdr) + vorbtag_len + 1;
}


#define ERR(v, n) \
	(v)->err = n,  FFVORBIS_RERR

static const char* const _ffvorbis_errs[] = {
	"",
	"unsupported input audio format",
	"bad packet",
	"invalid tags",
};

const char* _ffvorbis_errstr(int e)
{
	if (e == FFVORBIS_ESYS)
		return fferr_strptr(fferr_last());

	if (e >= 0)
		return _ffvorbis_errs[e];
	return vorbis_errstr(e);
}


/** Initialize ffvorbis. */
int ffvorbis_open(ffvorbis *v)
{
	v->seek_sample = (uint64)-1;
	v->total_samples = (uint64)-1;
	return 0;
}

void ffvorbis_close(ffvorbis *v)
{
	if (v->vctx) {
		vorbis_decode_free(v->vctx);
		v->vctx = NULL;
	}
}

void ffvorbis_seek(ffvorbis *v, uint64 sample)
{
	v->seek_sample = sample;
}

/** Decode Vorbis packet.
Return enum FFVORBIS_R. */
int ffvorbis_decode(ffvorbis *v, const char *pkt, size_t len)
{
	enum { R_HDR, R_TAGS, R_BOOK, R_DATA };
	int r;

	if (len == 0)
		return FFVORBIS_RMORE;

	ogg_packet opkt = {};
	opkt.packet = (void*)pkt,  opkt.bytes = len;
	opkt.packetno = v->pktno++;
	opkt.granulepos = -1;
	opkt.e_o_s = v->fin;

	switch (v->state) {
	case R_HDR:
		if (0 != vorb_info(pkt, len, &v->info.channels, &v->info.rate, &v->info.bitrate_nominal))
			return ERR(v, FFVORBIS_EPKT);

		if (0 != (r = vorbis_decode_init(&v->vctx, &opkt)))
			return ERR(v, r);
		v->state = R_TAGS;
		return FFVORBIS_RHDR;

	case R_TAGS:
		if (NULL == vorb_comm(pkt, len, NULL))
			return ERR(v, FFVORBIS_ETAG);
		v->state = R_BOOK;
		return FFVORBIS_RHDRFIN;

	case R_BOOK:
		if (0 != (r = vorbis_decode_init(&v->vctx, &opkt)))
			return ERR(v, r);
		v->state = R_DATA;
		return FFVORBIS_RMORE;
	}

	r = vorbis_decode(v->vctx, &opkt, &v->pcm);
	if (r < 0)
		return v->err = r,  FFVORBIS_RWARN;
	else if (r == 0)
		return FFVORBIS_RMORE;

	if (v->seek_sample != (uint64)-1) {
		if (v->seek_sample < v->cursample) {
			//couldn't find the target packet within the page
			v->seek_sample = v->cursample;
		}

		uint skip = ffmin(v->seek_sample - v->cursample, r);
		v->cursample += skip;
		if (v->cursample != v->seek_sample || (uint)r == skip)
			return FFVORBIS_RMORE; //not yet reached the target packet

		v->seek_sample = (uint64)-1;
		for (uint i = 0;  i != v->info.channels;  i++) {
			v->pcm_arr[i] = v->pcm[i] + skip;
		}
		v->pcm = v->pcm_arr;
		r -= skip;
	}

	if (v->total_samples != (uint64)-1)
		r = ffmin(r, v->total_samples - v->cursample);
	v->pkt_samples = r;
	v->pcmlen = r * sizeof(float) * v->info.channels;
	v->cursample += r;
	return FFVORBIS_RDATA;
}


typedef struct ffvorbis_enc {
	uint state;
	vorbis_ctx *vctx;
	uint channels;
	uint sample_rate;
	uint quality;
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
@quality: q*10
*/
int ffvorbis_create(ffvorbis_enc *v, const struct phi_af *fmt, int quality)
{
	int r;

	if (fmt->format != PHI_PCM_FLOAT32)
		return ERR(v, FFVORBIS_EFMT);

	vorbis_encode_params params = {0};
	params.channels = fmt->channels;
	params.rate = fmt->rate;
	params.quality = (float)quality / 100;
	ogg_packet pkt[2];
	if (0 != (r = vorbis_encode_create(&v->vctx, &params, &pkt[0], &pkt[1])))
		return ERR(v, r);

	const char *svendor = vorbis_vendor();
	ffstr vendor = FFSTR_INITZ(svendor);
	vorbistagwrite_add(&v->vtag, MMTAG_VENDOR, vendor);

	v->channels = fmt->channels;
	v->sample_rate = fmt->rate;
	v->quality = quality;
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

static const ffbyte _ffvorbis_brates[] = {
	45/2, 64/2, 80/2, 96/2, 112/2, 128/2, 160/2, 192/2, 224/2, 256/2, 320/2, 500/2 //q=-1..10 for 44.1kHz mono
};

/** Get bitrate from quality. */
uint ffvorbis_enc_bitrate(ffvorbis_enc *v, int quality)
{
	uint q = quality / 10 + 1;
	return _ffvorbis_brates[q] * 1000 * v->channels;
}

/** Get approximate output file size. */
uint64 ffvorbis_enc_size(ffvorbis_enc *v, uint64 total_samples)
{
	return (total_samples / v->sample_rate + 1) * (ffvorbis_enc_bitrate(v, v->quality) / 8);
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
