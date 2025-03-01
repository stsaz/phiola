/** FLAC encoder interface
2015, Simon Zolin */

#pragma once
#include <ffbase/vector.h>
#include <avpack/base/flac.h>
#include <FLAC/FLAC-phi.h>

enum {
	FFFLAC_RDATA,
	FFFLAC_RMORE,
	FFFLAC_RDONE,
	FFFLAC_RERR,
};

enum {
	FLAC_ELIB = 1,
	FLAC_ESYS,
};

#define ERR(f, e) \
	(f)->errtype = e,  FFFLAC_RERR

static inline int int_ltoh24s(const void *p)
{
	const ffbyte *b = (ffbyte*)p;
	uint n = ((uint)b[2] << 16) | ((uint)b[1] << 8) | b[0];
	if (n & 0x00800000)
		n |= 0xff000000;
	return n;
}

/** Convert data between 32bit integer and any other integer PCM format.
e.g. 16bit: "11 22 00 00" <-> "11 22" */

static int pcm_to32(int **dst, const void **src, uint srcbits, uint channels, uint samples)
{
	uint ic, i;
	union {
	char **pb;
	short **psh;
	} from;
	from.psh = (void*)src;

	switch (srcbits) {
	case 8:
		for (ic = 0;  ic != channels;  ic++) {
			for (i = 0;  i != samples;  i++) {
				dst[ic][i] = from.pb[ic][i];
			}
		}
		break;

	case 16:
		for (ic = 0;  ic != channels;  ic++) {
			for (i = 0;  i != samples;  i++) {
				dst[ic][i] = from.psh[ic][i];
			}
		}
		break;

	case 24:
		for (ic = 0;  ic != channels;  ic++) {
			for (i = 0;  i != samples;  i++) {
				dst[ic][i] = int_ltoh24s(&from.pb[ic][i * 3]);
			}
		}
		break;

	default:
		return -1;
	}

	return 0;
}

enum FFFLAC_ENC_OPT {
	FFFLAC_ENC_NOMD5 = 1, // don't generate MD5 checksum of uncompressed data
};

typedef struct ffflac_enc {
	uint state;
	flac_encoder *enc;
	struct flac_info info;
	uint err;
	uint errtype;

	size_t datalen;
	const ffbyte *data;

	size_t pcmlen;
	const void **pcm;
	uint frsamps;
	ffvec outbuf;
	int* pcm32[FLAC__MAX_CHANNELS];
	size_t off_pcm
		, off_pcm32
		, cap_pcm32;

	uint level; //0..8.  Default: 5.
	uint fin :1;

	uint opts; //enum FFFLAC_ENC_OPT
} ffflac_enc;

void ffflac_enc_init(ffflac_enc *f)
{
	ffmem_zero_obj(f);
}

void ffflac_enc_close(ffflac_enc *f)
{
	ffvec_free(&f->outbuf);
	if (f->enc) {
		flac_encode_free(f->enc);
		f->enc = NULL;
	}
}

#define ffflac_enc_fin(f)  ((f)->fin = 1)

const char* ffflac_enc_errstr(ffflac_enc *f)
{
	switch (f->errtype) {
	case FLAC_ESYS:
		return "not enough memory";

	case FLAC_ELIB:
		return flac_errstr(f->err);
	}

	return "";
}

enum ENC_STATE {
	ENC_HDR, ENC_FRAMES, ENC_DONE
};

/** Return 0 on success. */
int ffflac_create(ffflac_enc *f, flac_conf *conf)
{
	int r;
	if (0 != (r = flac_encode_init(&f->enc, conf))) {
		f->err = r;
		f->errtype = FLAC_ELIB;
		return FLAC_ELIB;
	}

	f->info.bits = conf->bps;
	f->info.channels = conf->channels;
	f->info.sample_rate = conf->rate;

	flac_conf info;
	flac_encode_info(f->enc, &info);
	f->info.minblock = info.min_blocksize;
	f->info.maxblock = info.max_blocksize;
	return 0;
}

/** Return enum FFFLAC_R. */
/*
Encode audio data into FLAC frames.
  An input sample must be within 32-bit container.
  To encode a frame libFLAC needs NBLOCK+1 input samples.
  flac_encode() returns a frame with NBLOCK encoded samples,
    so 1 sample always stays cached in libFLAC until we explicitly flush output data.
*/
int ffflac_encode(ffflac_enc *f)
{
	uint samples, sampsize, blksize;
	int r;

	switch (f->state) {

	case ENC_HDR:
		if (NULL == ffvec_realloc(&f->outbuf, (f->info.minblock + 1) * sizeof(int) * f->info.channels, 1))
			return ERR(f, FLAC_ESYS);
		for (uint i = 0;  i != f->info.channels;  i++) {
			f->pcm32[i] = (void*)(f->outbuf.ptr + (f->info.minblock + 1) * sizeof(int) * i);
		}
		f->cap_pcm32 = f->info.minblock + 1;

		f->state = ENC_FRAMES;
		// fallthrough

	case ENC_FRAMES:
		break;

	case ENC_DONE: {
		flac_conf info = {};
		flac_encode_info(f->enc, &info);
		f->info.minblock = info.min_blocksize;
		f->info.maxblock = info.max_blocksize;
		f->info.minframe = info.min_framesize;
		f->info.maxframe = info.max_framesize;
		ffmem_copy(f->info.md5, info.md5, sizeof(f->info.md5));
		return FFFLAC_RDONE;
	}
	}

	sampsize = f->info.bits/8 * f->info.channels;
	samples = ffmin(f->pcmlen / sampsize - f->off_pcm, f->cap_pcm32 - f->off_pcm32);

	if (samples == 0 && !f->fin) {
		f->off_pcm = 0;
		return FFFLAC_RMORE;
	}

	if (samples != 0) {
		const void* src[FLAC__MAX_CHANNELS];
		int* dst[FLAC__MAX_CHANNELS];

		for (uint i = 0;  i != f->info.channels;  i++) {
			src[i] = (char*)f->pcm[i] + f->off_pcm * f->info.bits/8;
			dst[i] = f->pcm32[i] + f->off_pcm32;
		}

		r = pcm_to32(dst, src, f->info.bits, f->info.channels, samples);
		FF_ASSERT(!r);

		f->off_pcm += samples;
		f->off_pcm32 += samples;
		if (!(f->off_pcm32 == f->cap_pcm32 || f->fin)) {
			f->off_pcm = 0;
			return FFFLAC_RMORE;
		}
	}

	samples = f->off_pcm32;
	f->off_pcm32 = 0;
	r = flac_encode(f->enc, (const int**)f->pcm32, &samples, (char**)&f->data);
	if (r < 0)
		return f->errtype = FLAC_ELIB,  f->err = r,  FFFLAC_RERR;

	blksize = f->info.minblock;
	if (r == 0 && f->fin) {
		samples = 0;
		r = flac_encode(f->enc, (const int**)f->pcm32, &samples, (char**)&f->data);
		if (r < 0)
			return f->errtype = FLAC_ELIB,  f->err = r,  FFFLAC_RERR;
		blksize = samples;
		f->state = ENC_DONE;
	}

	FF_ASSERT(r != 0);
	FF_ASSERT(samples == f->cap_pcm32 || f->fin);

	if (f->cap_pcm32 == f->info.minblock + 1)
		f->cap_pcm32 = f->info.minblock;

	f->frsamps = blksize;
	f->datalen = r;
	return FFFLAC_RDATA;
}
