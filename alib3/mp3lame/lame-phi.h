/** libmp3lame interface
2016, Simon Zolin */

#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef struct lame lame;

struct lame_params {
	unsigned format;
	unsigned interleaved;
	unsigned channels;
	unsigned rate;
	unsigned quality;
};

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT const char* lame_errstr(int e);

/**
Return 0 on success. */
_EXPORT int lame_create(lame **lm, struct lame_params *conf);

_EXPORT void lame_free(lame *lm);

/**
Return the number of bytes written;
 0 if more data is needed;
 <0 on error. */
_EXPORT int lame_encode(lame *lm, const void **pcm, unsigned int samples, char *buf, size_t cap);

/** Get LAME-tag frame. */
_EXPORT int lame_lametag(lame *lm, char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
