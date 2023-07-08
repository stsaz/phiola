/** phiola: utility functions
2023, Simon Zolin */

#include <ffbase/stringz.h>

struct map_sz_vptr {
	const char *key;
	const void *val;
};
static inline const void* map_sz_vptr_find(const struct map_sz_vptr *m, const char *name)
{
	for (uint i = 0;  m[i].key != NULL;  i++) {
		if (ffsz_eq(m[i].key, name))
			return m[i].val;
	}
	return NULL;
}


#include <FFOS/path.h>
static inline void ffpath_split3_str(ffstr fullname, ffstr *path, ffstr *name, ffstr *ext)
{
	ffstr nm;
	ffpath_splitpath_str(fullname, path, &nm);
	ffpath_splitname_str(nm, name, ext);
}


#define msec_to_samples(time_ms, rate)   ((uint64)(time_ms) * (rate) / 1000)

/** bits per sample */
#define pcm_bits(format)  ((format) & 0xff)

/** Get size of 1 sample (in bytes). */
#define pcm_size(format, channels)  (pcm_bits(format) / 8 * (channels))
#define pcm_size1(f)  pcm_size((f)->format, (f)->channels)
