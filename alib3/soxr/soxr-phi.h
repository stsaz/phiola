/** libsoxr wrapper */

#pragma once
#include <stdint.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
	#include <sys/types.h>
#endif

typedef struct soxr_ctx soxr_ctx;

enum SOXR_FMT {
	SOXR_I16 = 16,
	SOXR_I32 = 32,
	SOXR_F32 = 0x0100 | 32,
	SOXR_F64 = 0x0100 | 64,
};

enum SOXR_F {
	SOXR_F_NO_DITHER = 1,
};

struct soxr_conf {
	unsigned i_rate, o_rate;
	unsigned i_format, o_format; // enum SOXR_FMT
	unsigned i_interleaved, o_interleaved;
	unsigned channels;

	unsigned quality;
	unsigned flags; // enum SOXR_F
	const char *error;
};

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT int phi_soxr_create(soxr_ctx **c, struct soxr_conf *conf);

_EXPORT void phi_soxr_destroy(soxr_ctx *c);

/**
Flush: input=NULL, len=0, *off=0
Return N of bytes written;
	<0: error. */
_EXPORT int phi_soxr_convert(soxr_ctx *c, const void *input, size_t len, size_t *off, void *output, size_t cap);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
