/** FLAC decoder interface
2015, Simon Zolin */

#pragma once
#include <avpack/flac-fmt.h>
#include <FLAC/FLAC-phi.h>

typedef struct ffflac_dec {
	flac_decoder *dec;
	int err;
	flac_conf info;
	uint64 frsample;
	ffstr in;

	size_t pcmlen;
	void *out[FLAC__MAX_CHANNELS];
} ffflac_dec;

/** Return 0 on success */
int ffflac_dec_open(ffflac_dec *f, flac_conf *info)
{
	int r;
	if (0 != (r = flac_decode_init(&f->dec, info))) {
		f->err = r;
		return -1;
	}
	f->info = *info;
	f->frsample = ~0ULL;
	return 0;
}

void ffflac_dec_close(ffflac_dec *f)
{
	if (f->dec != NULL)
		flac_decode_free(f->dec);
}

/** Set input data */
static inline void ffflac_dec_input(ffflac_dec *f, ffstr frame, uint frame_samples)
{
	f->in = frame;
	f->pcmlen = frame_samples;
}

static inline void int_htol24(void *p, uint n)
{
	ffbyte *o = (ffbyte*)p;
	o[0] = (ffbyte)n;
	o[1] = (ffbyte)(n >> 8);
	o[2] = (ffbyte)(n >> 16);
}

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

const char* ffflac_dec_errstr(ffflac_dec *f)
{
	return flac_errstr(f->err);
}

/**
output: output data (non-interleaved PCM)
pos: absolute sample position
Return enum FFFLAC_R */
int ffflac_decode(ffflac_dec *f, ffstr *output, uint64 *pos)
{
	const int **out;
	int r = flac_decode(f->dec, f->in.ptr, f->in.len, &out);
	if (r != 0) {
		f->err = r;
		return -1;
	}

	for (uint i = 0;  i != f->info.channels;  i++) {
		f->out[i] = (void*)out[i];
	}

	//in-place conversion
	pcm_from32(out, f->out, f->info.bps, f->info.channels, f->pcmlen);
	ffstr_set(output, f->out, f->pcmlen * f->info.bps / 8 * f->info.channels);
	*pos = f->frsample;
	f->frsample += f->pcmlen;
	return 0;
}
