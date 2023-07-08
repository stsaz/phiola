/** libmpc interface
2017, Simon Zolin */

#pragma once
#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef struct mpc_ctx mpc_ctx;
typedef struct mpc_seekctx mpc_seekctx;

enum {
	MPC_FRAME_SAMPLES = 36 * 32,
	MPC_ABUF_CAP = MPC_FRAME_SAMPLES * 2 * sizeof(float), // required capacity of output audio buffer
	MPC_FRAME_MAXSIZE = 4352, // max. frame size
};

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT const char* mpc_errstr(int e);


/** Initialize decoder.
sh_block: SH block body
Return 0 on success;
 <0: error. */
_EXPORT int mpc_decode_open(mpc_ctx **pc, const void *sh_block, size_t len);

_EXPORT void mpc_decode_free(mpc_ctx *c);

_EXPORT void mpc_decode_input(mpc_ctx *c, const void *block, size_t len);

/** Decode one frame in a block.
pcm: output samples.  Its size must be MPC_ABUF_CAP.
Return the number of audio samples decoded;
 0 if need next block;
 <0 on error. */
_EXPORT int mpc_decode(mpc_ctx *c, float *pcm);


/** Build seek table from ST block data.
Return 0 on success;
 <0: error. */
_EXPORT int mpc_seekinit(mpc_seekctx **pc, const void *sh_block, size_t sh_len, const void *st_block, size_t st_len);

_EXPORT void mpc_seekfree(mpc_seekctx *c);

/** Get the nearest file offset by block number.
Return file offset;
 0 on error. */
_EXPORT long long mpc_seek(mpc_seekctx *c, unsigned int *blk);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
