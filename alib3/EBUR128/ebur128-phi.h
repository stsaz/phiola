/** libEBUR128 wrapper */

#pragma once
#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
	#include <sys/types.h>
#endif

typedef struct ebur128_ctx ebur128_ctx;

enum EBUR128_PROPERTY {
	EBUR128_LOUDNESS_MOMENTARY = 1,
	EBUR128_LOUDNESS_SHORTTERM = 2,
	EBUR128_LOUDNESS_GLOBAL = 4,
	EBUR128_LOUDNESS_RANGE = 8,
	EBUR128_SAMPLE_PEAK = 0x10,
};

struct ebur128_conf {
	unsigned channels;
	unsigned sample_rate;
	unsigned mode; // enum EBUR128_PROPERTY
};

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT int ebur128_open(ebur128_ctx **c, struct ebur128_conf *conf);

_EXPORT void ebur128_close(ebur128_ctx *c);

_EXPORT void ebur128_process(ebur128_ctx *c, const double *data, size_t len);

/**
what: enum EBUR128_PROPERTY */
_EXPORT int ebur128_get(ebur128_ctx *c, unsigned what, void *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
