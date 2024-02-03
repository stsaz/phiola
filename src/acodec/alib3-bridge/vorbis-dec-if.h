/** Vorbis decoder interface
2016, Simon Zolin */

/*
OGG(VORB_INFO)  OGG(VORB_COMMENTS VORB_CODEBOOK)  OGG(PKT1 PKT2...)...
*/

#pragma once
#include <afilter/pcm.h>
#include <ffbase/string.h>
#include <avpack/vorbistag.h>
#include <vorbis/vorbis-phi.h>

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
	uint64 cursample;

	const float **pcm; //non-interleaved
	uint fin :1;
} ffvorbis;

#define ffvorbis_errstr(v)  _ffvorbis_errstr((v)->err)

/** Get bps. */
#define ffvorbis_bitrate(v)  ((v)->info.bitrate_nominal)

#define ffvorbis_rate(v)  ((v)->info.rate)
#define ffvorbis_channels(v)  ((v)->info.channels)

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
	v->cursample = ~0ULL;
	return 0;
}

void ffvorbis_close(ffvorbis *v)
{
	if (v->vctx) {
		vorbis_decode_free(v->vctx);
		v->vctx = NULL;
	}
}

/** Decode Vorbis packet.
Return enum FFVORBIS_R. */
int ffvorbis_decode(ffvorbis *v, ffstr pkt, ffstr *out, uint64 *pos)
{
	enum { R_HDR, R_TAGS, R_BOOK, R_DATA };
	int r;

	if (pkt.len == 0)
		return FFVORBIS_RMORE;

	ogg_packet opkt = {};
	opkt.packet = (void*)pkt.ptr,  opkt.bytes = pkt.len;
	opkt.packetno = v->pktno++;
	opkt.granulepos = -1;
	opkt.e_o_s = v->fin;

	switch (v->state) {
	case R_HDR:
		if (0 != vorb_info(pkt.ptr, pkt.len, &v->info.channels, &v->info.rate, &v->info.bitrate_nominal))
			return ERR(v, FFVORBIS_EPKT);

		if (0 != (r = vorbis_decode_init(&v->vctx, &opkt)))
			return ERR(v, r);
		v->state = R_TAGS;
		return FFVORBIS_RHDR;

	case R_TAGS:
		if (NULL == vorb_comm(pkt.ptr, pkt.len, NULL))
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

	ffstr_set(out, (void*)v->pcm, r * sizeof(float) * v->info.channels);
	*pos = v->cursample;
	v->cursample += r;
	return FFVORBIS_RDATA;
}
