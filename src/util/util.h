/** phiola: utility functions
2023, Simon Zolin */

#include <ffbase/stringz.h>

static inline ffbool ffbit_test_array32(const uint *ar, uint bit)
{
	return ffbit_test32(&ar[bit / 32], bit % 32);
}


/** Process input string of the format "...text...$var...text...".
out: either text chunk or variable name */
static inline int ffstr_var_next(ffstr *in, ffstr *out, char c)
{
	if (in->ptr[0] != c) {
		ffssize pos = ffstr_findchar(in, c);
		if (pos < 0)
			pos = in->len;
		ffstr_set(out, in->ptr, pos);
		ffstr_shift(in, pos);
		return 't';
	}

	ffstr_shift(in, 1);
	ffssize i = ffs_skip_ranges(in->ptr, in->len, "\x30\x39\x41\x5a\x5f\x5f\x61\x7a", 8); // "0-9A-Z_a-z"
	if (i < 0)
		i = in->len;
	ffstr_set(out, in->ptr - 1, i + 1);
	ffstr_shift(in, i);
	return 'v';
}


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
static inline const void* map_sz_vptr_findstr(const struct map_sz_vptr *m, ffstr name)
{
	for (uint i = 0;  m[i].key != NULL;  i++) {
		if (ffstr_eqz(&name, m[i].key))
			return m[i].val;
	}
	return NULL;
}


#include <ffsys/path.h>
static inline void ffpath_split3_str(ffstr fullname, ffstr *path, ffstr *name, ffstr *ext)
{
	ffstr nm;
	ffpath_splitpath_str(fullname, path, &nm);
	ffpath_splitname_str(nm, name, ext);
}

/** Treat ".ext" file name as just an extension without a name */
static inline void ffpath_split3_output(ffstr fullname, ffstr *path, ffstr *name, ffstr *ext)
{
	ffstr nm;
	ffpath_splitpath_str(fullname, path, &nm);
	ffstr_rsplitby(&nm, '.', name, ext);
}

/** Replace uncommon/invalid characters in file name component
Return N bytes written */
static inline ffsize ffpath_makefn(char *dst, ffsize dstcap, ffstr src, char replace_char, const uint *char_bitmask)
{
	ffstr_skipchar(&src, ' ');
	ffstr_rskipchar(&src, ' ');
	ffsize len = ffmin(src.len, dstcap);

	for (ffsize i = 0;  i < len;  i++) {
		dst[i] = src.ptr[i];
		if (!ffbit_test_array32(char_bitmask, (ffbyte)src.ptr[i]))
			dst[i] = replace_char;
	}
	return len;
}


#define samples_to_msec(samples, rate)   ((uint64)(samples) * 1000 / (rate))
#define msec_to_samples(time_ms, rate)   ((uint64)(time_ms) * (rate) / 1000)

/** bits per sample */
#define pcm_bits(format)  ((format) & 0xff)

/** Get size of 1 sample (in bytes). */
#define pcm_size(format, channels)  (pcm_bits(format) / 8 * (channels))
#define pcm_size1(f)  pcm_size((f)->format, (f)->channels)
