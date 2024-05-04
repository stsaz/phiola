/** libFLAC interface
2016, Simon Zolin */

#include "FLAC-phi.h"
#include <config.h>
#include <FLAC/export.h>
#include <FLAC/format.h>
#include <FLAC/stream_decoder.h>
#include <FLAC/stream_encoder.h>
#include <private/crc.h>
#include <memory.h>

const char* flac_errstr(int err)
{
	err = -err;
	int e = err & 0xff;
	switch ((err >> 8) & 0xff) {
	case FLAC_E_RINIT:
		return FLAC__StreamDecoderInitStatusString[e];

	case FLAC_E_RSTATUS:
		return FLAC__StreamDecoderErrorStatusString[e];

	case FLAC_E_RSTATE:
		return FLAC__StreamDecoderStateString[e];

	case FLAC_E_WINIT:
		return FLAC__StreamEncoderInitStatusString[e];

	case FLAC_E_WSTATE:
		return FLAC__StreamEncoderStateString[e];
	}
	return "unknown error";
}

const char* flac_vendor(void)
{
	return FLAC__VENDOR_STRING;
}


struct flac_decoder {
	FLAC__StreamDecoder *decoder;
};

extern int _flac_decode_init(FLAC__StreamDecoder *d);
extern FLAC__StreamMetadata_StreamInfo* _flac_decode_info(FLAC__StreamDecoder *d);
extern int _flac_decode(FLAC__StreamDecoder *d, const char *input, size_t len, const int ***output);

int flac_decode_init(flac_decoder **pf, flac_conf *conf)
{
	int r;
	flac_decoder *f;

	if (NULL == (f = calloc(1, sizeof(flac_decoder)))) {
		r = FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR;
		goto err;
	}

	if (NULL == (f->decoder = FLAC__stream_decoder_new())) {
		r = FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR;
		goto err;
	}

	if (0 != (r = _flac_decode_init(f->decoder))) {
		FLAC__stream_decoder_delete(f->decoder);
		goto err;
	}

	FLAC__StreamMetadata_StreamInfo *si = _flac_decode_info(f->decoder);
	memset(si, 0, sizeof(*si));
	si->min_blocksize = conf->min_blocksize;
	si->max_blocksize = conf->max_blocksize;
	si->bits_per_sample = conf->bps;
	si->channels = conf->channels;
	si->sample_rate = conf->rate;

	*pf = f;
	return 0;

err:
	free(f);
	return FLAC_ERR(FLAC_E_RINIT, r);
}

void flac_decode_free(flac_decoder *f)
{
	FLAC__stream_decoder_delete(f->decoder);
	free(f);
}

int flac_decode(flac_decoder *f, const char *input, size_t len, const int ***output)
{
	return _flac_decode(f->decoder, input, len, output);
}

#ifdef _WIN32
FILE* fopen_utf8(const char *filename, const char *mode){}
int rename_utf8(const char *oldname, const char *newname){}
int utime_utf8(const char *filename, struct utimbuf *times){}
int unlink_utf8(const char *filename){}
int stat64_utf8(const char *path, struct __stat64 *buffer){}
int chmod_utf8(const char *filename, int pmode){}
#endif


struct flac_encoder {
	FLAC__StreamEncoder *encoder;
};

extern int _flac_encode_init(FLAC__StreamEncoder *enc, flac_conf *conf);
extern FLAC__StreamMetadata_StreamInfo* _flac_encode_info(FLAC__StreamEncoder *enc);
extern int _flac_encode(FLAC__StreamEncoder *enc, const int * const *audio, unsigned int *samples, char **buf);

int flac_encode_init(flac_encoder **pf, flac_conf *conf)
{
	flac_encoder *f;
	int r;

	if (NULL == (f = calloc(1, sizeof(flac_encoder)))) {
		r = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
		goto err;
	}

	if (NULL == (f->encoder = FLAC__stream_encoder_new())) {
		r = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
		goto err;
	}

	if (0 != (r = _flac_encode_init(f->encoder, conf))) {
		flac_encode_free(f);
		return r;
	}

	*pf = f;
	return 0;

err:
	free(f);
	return FLAC_ERR(FLAC_E_WSTATE, r);
}

void flac_encode_free(flac_encoder *f)
{
	FLAC__stream_encoder_delete(f->encoder);
	free(f);
}

void flac_encode_info(flac_encoder *f, flac_conf *info)
{
	const FLAC__StreamMetadata_StreamInfo *si = _flac_encode_info(f->encoder);
	info->min_blocksize = si->min_blocksize;
	info->max_blocksize = si->max_blocksize;
	info->min_framesize = si->min_framesize;
	info->max_framesize = si->max_framesize;
	memcpy(info->md5, si->md5sum, sizeof(info->md5));
}

int flac_encode(flac_encoder *f, const int * const *audio, unsigned int *samples, char **buf)
{
	return _flac_encode(f->encoder, audio, samples, buf);
}
