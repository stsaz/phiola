/** libFLAC interface
2016, Simon Zolin */

#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef struct flac_decoder flac_decoder;
typedef struct flac_encoder flac_encoder;

#ifndef FLAC_EXP
#define FLAC__MAX_CHANNELS 8
#endif

typedef struct flac_conf {
	unsigned int bps;
	unsigned int channels;
	unsigned int rate;

	unsigned int level;
	unsigned int nomd5;
	char md5[16];
	unsigned int min_blocksize, max_blocksize;
	unsigned int min_framesize, max_framesize;
} flac_conf;


#ifdef __cplusplus
extern "C" {
#endif

_EXPORT const char* flac_vendor(void);

_EXPORT const char* flac_errstr(int err);


/**
Return 0 on success. */
_EXPORT int flac_decode_init(flac_decoder **dec, flac_conf *conf);

_EXPORT void flac_decode_free(flac_decoder *dec);

/** Decode 1 frame.
Return 0 on success. */
_EXPORT int flac_decode(flac_decoder *dec, const char *data, size_t len, const int ***audio);


/**
Return 0 on success. */
_EXPORT int flac_encode_init(flac_encoder **enc, flac_conf *conf);

_EXPORT void flac_encode_free(flac_encoder *enc);

/**
@samples: [in] samples in "audio"; [out] samples processed.
Return the number of bytes written;
 0 if more data is needed;
 <0: error. */
_EXPORT int flac_encode(flac_encoder *enc, const int * const *audio, unsigned int *samples, char **buf);

/** Get stream info. */
_EXPORT void flac_encode_info(flac_encoder *enc, flac_conf *info);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
