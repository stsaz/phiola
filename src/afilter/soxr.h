/** phiola: libsoxr wrapper
2015, Simon Zolin */

#include <afilter/pcm.h>
#include <soxr/soxr.h>

typedef struct ffsoxr {
	soxr_t soxr;
	soxr_error_t err;
	uint isampsize
		, osampsize;
	uint nchannels;

	union {
	const void *in_i;
	const void **in;
	};
	uint inlen;
	uint inoff;

	union {
	void *out;
	void **outni;
	};
	uint outlen;
	uint outcap;

	uint quality; // 0..4. default:3 (High quality)
	uint in_ileaved :1
		, dither :1
		, fin :1; // the last block of input data
} ffsoxr;

static inline void ffsoxr_init(ffsoxr *soxr)
{
	ffmem_zero_obj(soxr);
	soxr->quality = SOXR_HQ;
}

#define ffsoxr_errstr(soxr)  soxr_strerror((soxr)->err)

static const ffbyte soxr_fmts[2][3] = {
	{ SOXR_INT16_S, SOXR_INT32_S, SOXR_FLOAT32_S },
	{ SOXR_INT16_I, SOXR_INT32_I, SOXR_FLOAT32_I },
};

static int _ffsoxr_getfmt(int fmt, int interleaved)
{
	switch (fmt) {
	case PHI_PCM_16:
		return soxr_fmts[interleaved][0];
	case PHI_PCM_32:
		return soxr_fmts[interleaved][1];
	case PHI_PCM_FLOAT32:
		return soxr_fmts[interleaved][2];
	}

	return -1;
}

/** Set array elements to point to consecutive regions of one buffer */
static inline void arrp_setbuf(void **ar, ffsize n, const void *buf, ffsize region_len)
{
	for (ffsize i = 0;  i != n;  i++) {
		ar[i] = (char*)buf + region_len * i;
	}
}

static inline int ffsoxr_create(ffsoxr *soxr, const struct phi_af *inpcm, const struct phi_af *outpcm)
{
	soxr_io_spec_t io;
	soxr_quality_spec_t qual;

	if (inpcm->channels != outpcm->channels)
		return -1;

	int itype = _ffsoxr_getfmt(inpcm->format, inpcm->interleaved);
	int otype = _ffsoxr_getfmt(outpcm->format, outpcm->interleaved);
	if (itype == -1 || otype == -1)
		return -1;
	io.itype = itype;
	io.otype = otype;
	io.scale = 1;
	io.e = NULL;
	io.flags = soxr->dither ? SOXR_TPDF : SOXR_NO_DITHER;

	qual = soxr_quality_spec(soxr->quality, SOXR_ROLLOFF_SMALL);

	soxr->soxr = soxr_create(inpcm->rate, outpcm->rate, inpcm->channels, &soxr->err
		, &io, &qual, NULL);
	if (soxr->err != NULL)
		return -1;

	soxr->isampsize = pcm_size1(inpcm);
	soxr->osampsize = pcm_size1(outpcm);
	soxr->outcap = outpcm->rate;
	if (NULL == (soxr->out = ffmem_alloc(sizeof(void*) * outpcm->channels + soxr->outcap * soxr->osampsize)))
		return -1;
	if (!outpcm->interleaved) {
		ffstr s;
		ffstr_set(&s, (char*)soxr->out + sizeof(void*) * outpcm->channels, soxr->outcap * pcm_bits(outpcm->format) / 8);
		// soxr->outni = soxr->out;
		arrp_setbuf(soxr->outni, outpcm->channels, s.ptr, s.len);
	}
	soxr->in_ileaved = inpcm->interleaved;
	soxr->nchannels = inpcm->channels;
	return 0;
}

static inline void ffsoxr_destroy(ffsoxr *soxr)
{
	ffmem_free(soxr->out);
	soxr_delete(soxr->soxr);
}

static inline int ffsoxr_convert(ffsoxr *soxr)
{
	size_t idone, odone;
	uint i, fin = 0;
	void *in = soxr->in;
	const void *inarr[8];

	for (;;) {

		if (soxr->inlen == 0) {
			if (!soxr->fin) {
				soxr->outlen = 0;
				return 0;
			}
			in = NULL;
			fin = 1;

		} else if (soxr->in_ileaved)
			in = (char*)soxr->in_i + soxr->inoff * soxr->isampsize;

		else {
			for (i = 0;  i != soxr->nchannels;  i++) {
				inarr[i] = (char*)soxr->in[i] + soxr->inoff * soxr->isampsize / soxr->nchannels;
			}
			in = inarr;
		}

		soxr->err = soxr_process(soxr->soxr, in, soxr->inlen / soxr->isampsize, &idone
			, soxr->out, soxr->outcap, &odone);
		if (soxr->err != NULL)
			return -1;

		soxr->inoff += idone;
		soxr->inlen -= idone * soxr->isampsize;
		if (soxr->inlen == 0)
			soxr->inoff = 0;
		soxr->outlen = odone * soxr->osampsize;

		if (odone != 0 || fin)
			break;
	}
	return 0;
}
