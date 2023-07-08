/** FLAC.
2015, Simon Zolin */

#pragma once
#include <afilter/pcm.h>
#include <ffbase/vector.h>
#include <avpack/flac-fmt.h>
#include <FLAC/FLAC-ff.h>

enum {
	FLAC_EFMT = 1,
	FLAC_ELIB,
	FLAC_ESYS,
};

enum {
	FFFLAC_RDATA,
	FFFLAC_RWARN,
	FFFLAC_RMORE,
	FFFLAC_RDONE,
	FFFLAC_RERR,
};

typedef struct ffflac_dec {
	flac_decoder *dec;
	int err;
	uint errtype;
	struct flac_info info;
	uint64 frsample;
	uint64 seeksample;
	ffstr in;

	size_t pcmlen;
	void **pcm;
	const void *out[FLAC__MAX_CHANNELS];
} ffflac_dec;

/** Return 0 on success. */
int ffflac_dec_open(ffflac_dec *f, const struct flac_info *info)
{
	int r;
	flac_conf si = {0};
	si.min_blocksize = info->minblock;
	si.max_blocksize = info->maxblock;
	si.rate = info->sample_rate;
	si.channels = info->channels;
	si.bps = info->bits;
	if (0 != (r = flac_decode_init(&f->dec, &si))) {
		f->errtype = FLAC_ELIB;
		f->err = r;
		return FFFLAC_RERR;
	}
	f->info = *info;
	return 0;
}

void ffflac_dec_close(ffflac_dec *f)
{
	if (f->dec != NULL)
		flac_decode_free(f->dec);
}

#define ffflac_dec_seek(f, sample) \
	(f)->seeksample = sample

/** Set input data. */
static FFINL void ffflac_dec_input(ffflac_dec *f, const ffstr *frame, uint frame_samples, uint64 frame_pos)
{
	f->in = *frame;
	f->frsample = frame_pos;
	f->pcmlen = frame_samples;
}

/** Get an absolute sample number. */
#define ffflac_dec_cursample(f)  ((f)->frsample)

/** Get output data (non-interleaved PCM). */
static FFINL size_t ffflac_dec_output(ffflac_dec *f, void ***pcm)
{
	*pcm = f->pcm;
	return f->pcmlen;
}

#define ERR(f, e) \
	(f)->errtype = e,  FFFLAC_RERR

/** Convert data between 32bit integer and any other integer PCM format.
e.g. 16bit: "11 22 00 00" <-> "11 22" */

static int pcm_from32(const int **src, void **dst, uint dstbits, uint channels, uint samples)
{
	uint ic, i;
	union {
	char **pb;
	short **psh;
	} to;
	to.psh = (void*)dst;

	switch (dstbits) {
	case 8:
		for (ic = 0;  ic != channels;  ic++) {
			for (i = 0;  i != samples;  i++) {
				to.pb[ic][i] = (char)src[ic][i];
			}
		}
		break;

	case 16:
		for (ic = 0;  ic != channels;  ic++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ic][i] = (short)src[ic][i];
			}
		}
		break;

	case 24:
		for (ic = 0;  ic != channels;  ic++) {
			for (i = 0;  i != samples;  i++) {
				int_htol24(&to.pb[ic][i * 3], src[ic][i]);
			}
		}
		break;

	default:
		return -1;
	}
	return 0;
}

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

const char* ffflac_dec_errstr(ffflac_dec *f)
{
	switch (f->errtype) {
	case FLAC_ESYS:
		return "not enough memory";

	case FLAC_ELIB:
		return flac_errstr(f->err);

	case FLAC_EFMT:
		return "format error";
	}

	return "";
}

/** Return enum FFFLAC_R. */
int ffflac_decode(ffflac_dec *f)
{
	int r;
	const int **out;
	uint isrc, ich;

	r = flac_decode(f->dec, f->in.ptr, f->in.len, &out);
	if (r != 0) {
		f->errtype = FLAC_ELIB;
		f->err = r;
		return FFFLAC_RWARN;
	}

	f->pcm = (void**)f->out;
	isrc = 0;
	if (f->seeksample != 0) {
		if (f->frsample < f->seeksample && f->seeksample < f->frsample + f->pcmlen) {
			isrc = f->seeksample - f->frsample;
			f->pcmlen -= isrc;
			f->frsample = f->seeksample;
		}
		f->seeksample = 0;
	}

	for (ich = 0;  ich != f->info.channels;  ich++) {
		f->out[ich] = out[ich];
	}

	const int *out2[FLAC__MAX_CHANNELS];
	for (ich = 0;  ich != f->info.channels;  ich++) {
		out2[ich] = out[ich] + isrc;
	}

	//in-place conversion
	pcm_from32(out2, (void*)f->out, f->info.bits, f->info.channels, f->pcmlen);
	f->pcmlen *= f->info.bits / 8 * f->info.channels;
	return FFFLAC_RDATA;
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
	f->level = 5;
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
	ffflac_dec fl;
	fl.errtype = f->errtype;
	fl.err = f->err;
	return ffflac_dec_errstr(&fl);
}

enum ENC_STATE {
	ENC_HDR, ENC_FRAMES, ENC_DONE
};

/** Return 0 on success. */
int ffflac_create(ffflac_enc *f, struct phi_af *pcm)
{
	int r;

	switch (pcm->format) {
	case PHI_PCM_8:
	case PHI_PCM_16:
	case PHI_PCM_24:
		break;

	default:
		pcm->format = PHI_PCM_24;
		f->errtype = FLAC_EFMT;
		return FLAC_EFMT;
	}

	flac_conf conf = {0};
	conf.bps = pcm_bits(pcm->format);
	conf.channels = pcm->channels;
	conf.rate = pcm->rate;
	conf.level = f->level;
	conf.nomd5 = !!(f->opts & FFFLAC_ENC_NOMD5);

	if (0 != (r = flac_encode_init(&f->enc, &conf))) {
		f->err = r;
		f->errtype = FLAC_ELIB;
		return FLAC_ELIB;
	}

	flac_conf info;
	flac_encode_info(f->enc, &info);
	f->info.minblock = info.min_blocksize;
	f->info.maxblock = info.max_blocksize;
	f->info.channels = pcm->channels;
	f->info.sample_rate = pcm->rate;
	f->info.bits = pcm_bits(pcm->format);
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
		flac_conf info = {0};
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

		if (0 != (r = pcm_to32(dst, src, f->info.bits, f->info.channels, samples)))
			return ERR(f, FLAC_EFMT);

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
