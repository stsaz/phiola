/** Opus decoder interface
2016, Simon Zolin */

/*
OGG(OPUS_HDR)  OGG(OPUS_TAGS)  OGG(OPUS_PKT...)...
*/

#pragma once
#include <avpack/vorbistag.h>
#include <opus/opus-phi.h>

enum FFOPUS_R {
	FFOPUS_RWARN = -2,
	FFOPUS_RERR = -1,
	FFOPUS_RHDR, //audio info is parsed
	FFOPUS_RHDRFIN, //header is finished
	FFOPUS_RHDRFIN_TAGS,
	FFOPUS_RDATA, //PCM data is returned
	FFOPUS_RMORE,
	FFOPUS_RDONE,
};


#define FFOPUS_HEAD_STR  "OpusHead"

typedef struct ffopus {
	uint state;
	int err;
	opus_ctx *dec;
	struct {
		uint channels;
		uint rate;
		uint orig_rate;
		uint preskip;
	} info;
	uint64 pos;
	ffvec pcmbuf;
} ffopus;

#define ffopus_errstr(o)  _ffopus_errstr((o)->err)

static inline void ffopus_setpos(ffopus *o, ffuint64 val)
{
	o->pos = val;
}

struct opus_hdr {
	char id[8]; //"OpusHead"
	ffbyte ver;
	ffbyte channels;
	ffbyte preskip[2];
	ffbyte orig_sample_rate[4];
	//ffbyte unused[3];
};

#define FFOPUS_TAGS_STR  "OpusTags"

#define ERR(o, n) \
	(o)->err = n,  FFOPUS_RERR

enum {
	FFOPUS_EHDR = 1,
	FFOPUS_EVER,
	FFOPUS_ETAG,

	FFOPUS_ESYS,
};

static const char* const _ffopus_errs[] = {
	"",
	"bad header",
	"unsupported version",
	"invalid tags",
};

const char* _ffopus_errstr(int e)
{
	if (e == FFOPUS_ESYS)
		return fferr_strptr(fferr_last());

	if (e >= 0)
		return _ffopus_errs[e];
	return opus_errstr(e);
}


int ffopus_open(ffopus *o)
{
	o->pos = ~0ULL;
	return 0;
}

void ffopus_close(ffopus *o)
{
	ffvec_free(&o->pcmbuf);
	if (o->dec) {
		opus_decode_free(o->dec);
		o->dec = NULL;
	}
}

/** Decode Opus packet.
pos: starting position (sample number) of the last decoded data
Return enum FFOPUS_R. */
int ffopus_decode(ffopus *o, ffstr *input, ffstr *output, uint64 *pos)
{
	enum { R_HDR, R_TAGS, R_DATA };
	int r;

	if (input->len == 0)
		return FFOPUS_RMORE;

	switch (o->state) {
	case R_HDR: {
		const struct opus_hdr *h = (struct opus_hdr*)input->ptr;
		if (input->len < sizeof(struct opus_hdr)
			|| memcmp(h->id, FFOPUS_HEAD_STR, 8))
			return ERR(o, FFOPUS_EHDR);

		if (h->ver != 1)
			return ERR(o, FFOPUS_EVER);

		o->info.channels = h->channels;
		o->info.rate = 48000;
		o->info.orig_rate = ffint_le_cpu32_ptr(h->orig_sample_rate);
		o->info.preskip = ffint_le_cpu16_ptr(h->preskip);

		opus_conf conf = {0};
		conf.channels = h->channels;
		r = opus_decode_init(&o->dec, &conf);
		if (r != 0)
			return ERR(o, r);

		if (NULL == ffvec_alloc(&o->pcmbuf, OPUS_BUFLEN(o->info.rate) * o->info.channels * sizeof(float), 1))
			return ERR(o, FFOPUS_ESYS);

		o->state = R_TAGS;
		input->len = 0;
		return FFOPUS_RHDR;
	}

	case R_TAGS:
		if (input->len < 8
			|| memcmp(input->ptr, FFOPUS_TAGS_STR, 8)) {
			o->state = R_DATA;
			return FFOPUS_RHDRFIN;
		}
		input->len = 0;
		o->state = R_DATA;
		return FFOPUS_RHDRFIN_TAGS;

	case R_DATA:
		break;
	}

	float *pcm = (void*)o->pcmbuf.ptr;
	r = opus_decode_f(o->dec, input->ptr, input->len, pcm);
	if (r < 0)
		return ERR(o, r);
	input->len = 0;

	ffstr_set(output, pcm, r * o->info.channels * sizeof(float));
	*pos = o->pos;
	o->pos += r;
	return FFOPUS_RDATA;
}
