/** FDK-AAC interface
2016, Simon Zolin */

#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef void fdkaac_decoder;
typedef struct fdkaac_encoder fdkaac_encoder;

enum AAC_MAX {
	AAC_MAXFRAMESAMPLES = 2048,
	AAC_MAXCHANNELS = 8,
};

enum AAC_AOT {
	AAC_LC = 2,
	AAC_HE = 5, //SBR
	AAC_HEV2 = 29, //SBR+PS
};

typedef struct fdkaac_conf {
	//input:
	unsigned int channels;
	unsigned int rate;
	unsigned int aot; //enum AAC_AOT
	unsigned int quality; // VBR:1..5, CBR:8000..800000 (bit/s) [in/out]
	unsigned int bandwidth; // up to 20000Hz [in/out]
	unsigned int afterburner :1;

	//output:
	unsigned int max_frame_size;
	unsigned int frame_samples;
	unsigned int enc_delay;
	char conf[64];
	unsigned int conf_len;
} fdkaac_conf;

typedef struct fdkaac_info {
	unsigned int aot; //enum AAC_AOT
	unsigned int channels;
	unsigned int rate;
	unsigned int bitrate;
} fdkaac_info;

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT const char* fdkaac_decode_errstr(int e);

/**
Return 0 on success;
 <0 on error. */
_EXPORT int fdkaac_decode_open(fdkaac_decoder **dec, const char *conf, size_t len);

_EXPORT void fdkaac_decode_free(fdkaac_decoder *dec);

/**
pcm: buffer with size = 2 * AAC_MAXCHANNELS * AAC_MAXFRAMESAMPLES (i.e. 2*8*2048)
Return the number of decoded samples;
 0 if more data is needed;
 <0 on error. */
_EXPORT int fdkaac_decode(fdkaac_decoder *dec, const char *data, size_t len, short *pcm);

/** Get frame information. */
_EXPORT int fdkaac_frameinfo(fdkaac_decoder *dec, fdkaac_info *info);


_EXPORT const char* fdkaac_encode_errstr(int e);

/**
Return 0 on success;
 <0 on error. */
_EXPORT int fdkaac_encode_create(fdkaac_encoder **penc, fdkaac_conf *conf);

_EXPORT void fdkaac_encode_free(fdkaac_encoder *enc);

/**
@samples: input/output audio samples;  -1: flush.
Return the number of bytes written;
 0 if more data is needed;
 <0 on error. */
_EXPORT int fdkaac_encode(fdkaac_encoder *enc, const short *audio, size_t *samples, char *data);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
