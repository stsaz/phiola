/** libmpg123 interface
2016, Simon Zolin */

#include <stdlib.h>

#ifdef WIN32
	#define PHI_EXPORT  __declspec(dllexport)
#else
	#define PHI_EXPORT  __attribute__((visibility("default")))
#endif

typedef struct phi_mpg123 phi_mpg123;

#ifdef __cplusplus
extern "C" {
#endif

PHI_EXPORT const char* phi_mpg123_error(int e);

PHI_EXPORT int phi_mpg123_init();

/**
Return 0 on success. */
PHI_EXPORT int phi_mpg123_open(phi_mpg123 **m, unsigned int flags);

PHI_EXPORT void phi_mpg123_free(phi_mpg123 *m);

/** Decode 1 MPEG frame.
Doesn't parse Xing tag.
audio: interleaved audio buffer
Return the number of bytes in audio buffer
 0 if more data is needed
 <0 on error */
PHI_EXPORT int phi_mpg123_decode(phi_mpg123 *m, const char *data, size_t size, unsigned char **audio);

/** Clear bufferred data */
PHI_EXPORT void phi_mpg123_reset(phi_mpg123 *m);

#ifdef __cplusplus
}
#endif

#undef PHI_EXPORT
