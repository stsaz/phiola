/** libMAC interface
2016, Simon Zolin */

#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef struct ape_decoder ape_decoder;

struct ape_info {
	int version;
	int compressionlevel;
	int bitspersample;
	int samplerate;
	int channels;
};

enum APE_ERROR {
	APE_EVERSION = 1,
	APE_EDATA,
	APE_ECRC,
	APE_EMOREDATA,
};

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT const char* ape_errstr(int e);

/**
Return 0 on success. */
_EXPORT int ape_decode_init(ape_decoder **a, const struct ape_info *info);

_EXPORT void ape_decode_free(ape_decoder *a);

/** Decode 1 frame.
Return the number of decoded samples;
 <0 on error. */
_EXPORT int ape_decode(ape_decoder *a, const char *data, size_t len, char *pcm, unsigned int samples, unsigned int align4);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
