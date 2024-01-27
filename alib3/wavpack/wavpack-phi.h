/** libwavpack interface
2016, Simon Zolin */

#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef struct wavpack_ctx wavpack_ctx;

typedef struct wavpack_info {
	unsigned int bps;
	unsigned int channels;
	unsigned int rate;
	unsigned int mode;
	unsigned char md5[16];
} wavpack_info;

#ifndef WVPK_EXP
/* wavpack.h */
#define MODE_WVC        0x1
#define MODE_LOSSLESS   0x2
#define MODE_HYBRID     0x4
#define MODE_FLOAT      0x8
#define MODE_VALID_TAG  0x10
#define MODE_HIGH       0x20
#define MODE_FAST       0x40
#define MODE_EXTRA      0x80    // extra mode used, see MODE_XMODE for possible level
#define MODE_APETAG     0x100
#define MODE_SFX        0x200
#define MODE_VERY_HIGH  0x400
#define MODE_MD5        0x800
#define MODE_XMODE      0x7000  // mask for extra level (1-6, 0=unknown)
#define MODE_DNS        0x8000
#endif

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT const char* wavpack_errstr(wavpack_ctx *w);

_EXPORT wavpack_ctx* wavpack_decode_init(void);

/**
Return 0 on success. */
_EXPORT int wavpack_read_header(wavpack_ctx *w, const char *data, size_t len, wavpack_info *info);

_EXPORT void wavpack_decode_free(wavpack_ctx *w);

/** Decode 1 block.
Return samples decoded;
 0 if the next block is needed;
 -1 on error. */
_EXPORT int wavpack_decode(wavpack_ctx *w, const char *data, size_t len, int *buffer, unsigned int samples);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
