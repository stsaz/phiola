/** libmpg123 interface
2016, Simon Zolin */

#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef struct mpg123 mpg123;

#ifndef MPG123_SO
enum MPG123_F {
	MPG123_FORCE_FLOAT = 0x400,
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT const char* mpg123_errstr(int e);

_EXPORT int mpg123_init(void);

/**
flags: enum MPG123_F.
Return 0 on success. */
_EXPORT int mpg123_open(mpg123 **m, unsigned int flags);

_EXPORT void mpg123_free(mpg123 *m);

/** Decode 1 MPEG frame.
Doesn't parse Xing tag.
audio: interleaved audio buffer
Return the number of bytes in audio buffer
 0 if more data is needed
 <0 on error */
_EXPORT int mpg123_decode(mpg123 *m, const char *data, size_t size, unsigned char **audio);

/** Clear bufferred data */
_EXPORT void mpg123_reset(mpg123 *m);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
