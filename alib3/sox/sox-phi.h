/** libsox wrapper */

#pragma once
#include <stdint.h>
#ifndef WIN32
#include <sys/types.h>
#endif

#define _EXPORT  __attribute__((visibility("default")))

typedef struct sox_ctx sox_ctx;

struct sox_conf {
	unsigned rate;
	unsigned channels;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
Return 0 on success. */
_EXPORT int phi_sox_create(sox_ctx **c, struct sox_conf *conf);

_EXPORT void phi_sox_destroy(sox_ctx *c);

/**
name: SoX filter name
 NULL: finalize filter chain
argv: the parameters to be passed to filter
Return 0 on success. */
_EXPORT int phi_sox_filter(sox_ctx *c, const char *name, const char* argv[], unsigned argc);

/**
len: [in] N of bytes in `input`; [out] N of bytes processed
output: [out] pointer to the output data
Return N of bytes written */
_EXPORT int phi_sox_process(sox_ctx *c, const int *input, size_t *len, int **output);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
