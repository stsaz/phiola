/** libALAC interface
2016, Simon Zolin */

#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef struct alac_ctx alac_ctx;

#ifdef __cplusplus
extern "C" {
#endif

/**
Return NULL on error. */
_EXPORT alac_ctx* alac_init(const char *magic_cookie, size_t len);

_EXPORT void alac_free(alac_ctx *a);

/** Decode 1 frame.
Return the number of decoded samples;
 0 if more data is needed;
 <0 on error. */
_EXPORT int alac_decode(alac_ctx *a, const char *data, size_t len, void *pcm);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
