/** Compact key-value storage
2026, Simon Zolin */

/*
ckv_destroy
ckv_set
ckv_list
ckv_find
ckv_copy
*/

#pragma once
#include <ffbase/string.h>

#ifndef CKV_STRUCT_SIZE
	#define CKV_STRUCT_SIZE  64
#endif

#define CKV_DATA_LIMIT  (100*1024*1024)

struct ckv {
	unsigned char n;
	char cache[CKV_STRUCT_SIZE-1-8]; // (len key\0 value\0)...

	/*
	cap[4]
	{
		size[4]
		len[4]
		key\0
		value\0
		padding[0..3]
	}...
	*/
	char *ptr;
};

#define CKV_CAP(c)  (((c)->ptr) ? *(unsigned*)(c)->ptr : 0)

enum CKV_LIST {
	CKV_F_UNIQUE = 1,	// Exclude rows with the same key
};

enum CKV_SET {
	// CKV_F_UNIQUE = 1	// Do nothing if the key exists
	CKV_F_REPLACE = 4,	// Replace existing key-value pair
	CKV_F_CACHE = 8		// Store data in cache (if it fits)
};

// enum CKV_FIND {
	// CKV_F_CACHE = 8	// Search in cache only
// };

enum CKV_E {
	CKV_E_OK,
	CKV_E_OK_CACHED,
	CKV_E_OK_REPLACED,
	CKV_E_DONE,			// No more rows to return
	CKV_E_EXISTS,		// Row with the same key already exists
	CKV_E_NOTEXIST,		// No row with such key
	CKV_E_LIMIT,		// Limit has been reached
	CKV_E_NOMEM,
};

#define _CKV_ALIGN  4
#define _CKV_KEY_EQ(kp, sz)  ffstr_ieqz(kp, sz)

#define _INT4_READ(p) \
({ \
	unsigned n = *(unsigned*)p; \
	p += 4; \
	n; \
})

#define _INT4_WRITE(p, n) \
	*(unsigned*)p = n; \
	p += 4

#define _INT1_READ(p) \
	*p++

#define _INT1_WRITE(p, n) \
	*p++ = n

static inline void ckv_destroy(struct ckv *c)
{
	c->n = c->cache[0] = 0;
	ffmem_free(c->ptr),  c->ptr = NULL;
}

static char* _ckv_cache_find(const char *d, const char *end, ffstr k)
{
	while (d < end) {
		unsigned n = _INT1_READ(d);
		if (!n)
			return NULL;
		FF_ASSERT(d + n <= end);
		if (_CKV_KEY_EQ(&k, d))
			return (char*)d - 1;
		d += n;
	}
	return NULL;
}

static char* _ckv_find(const char *d, const char *end, ffstr k)
{
	while (d < end) {
		unsigned size = _INT4_READ(d);
		_INT4_READ(d);
		FF_ASSERT(d + size <= end);
		if (_CKV_KEY_EQ(&k, d))
			return (char*)d - 8;
		d += size;
	}
	return NULL;
}

/**
flags: enum CKV_SET */
static inline int ckv_set(struct ckv *c, ffstr key, ffstr val, unsigned flags)
{
	int r = CKV_E_OK;
	ffstr k1, v1;
	unsigned cache_replace = 0, cap_cur, size;
	size_t cap, len;
	char *p;

	len = key.len + val.len + 2;
	if (c->n == 0xff || len >= CKV_DATA_LIMIT)
		return CKV_E_LIMIT;
	size = ffint_align_ceil2(len, _CKV_ALIGN);

	if ((flags & CKV_F_CACHE) && len + 1 > sizeof(c->cache))
		flags &= ~CKV_F_CACHE; // too large data to store in cache

	if (flags & (CKV_F_UNIQUE | CKV_F_REPLACE)) {
		if ((p = _ckv_cache_find(c->cache, c->cache + sizeof(c->cache), key))) {
			if (flags & CKV_F_UNIQUE)
				return CKV_E_EXISTS;

			if ((flags & CKV_F_CACHE)
				|| len + 1 <= sizeof(c->cache)) {
				// Case CR1/CR2a: replace data for the row with the same key
				// Note: only 1 row is stored in cache!
				_INT1_WRITE(p, len);
				p += key.len + 1;
				r = CKV_E_OK_CACHED;
				goto set_val;

			} else {
				// Case CR2b: remove the row from cache; add new row to data region
				// Note: only 1 row is stored in cache!
				// unsigned cache_len = *p;
				// ffmem_move(p, p + cache_len, sizeof(c->cache) - cache_len); // r1 r2 -> r2
				_INT1_WRITE(p, 0);
				r = CKV_E_OK_REPLACED;
				goto add;
			}
		}

		cap_cur = (c->ptr) ? *(unsigned*)c->ptr : 4;
		if ((p = _ckv_find(c->ptr + 4, c->ptr + cap_cur, key))) {
			r = CKV_E_OK_REPLACED;

			if (flags & CKV_F_UNIQUE)
				return CKV_E_EXISTS;

			unsigned size_cur = _INT4_READ(p);
			if (flags & CKV_F_CACHE) {
				// Case CR3: remove row; add new row to cache
				// Note: holes are not reused!
				_INT4_WRITE(p, 0);
				*p = '\0';

			} else {
				if (size <= size_cur) {
					// Case R1: replace value
					_INT4_WRITE(p, len);
					p += ffsz_len(p) + 1;
					goto set_val;
				}

				// Case R2: remove old row; add new row
				// Note: holes are not reused!
				_INT4_WRITE(p, 0);
				*p = '\0';
				goto add;
			}
		}
	}

	if (flags & CKV_F_CACHE) {
		r = CKV_E_OK_CACHED;
		p = c->cache;
		unsigned cache_len = *p;
		if (!cache_len) {
			// Case C1: add row to cache
			c->n++;
			_INT1_WRITE(p, len);
			goto set_pair;
		}

		// Case C2: push out the row from cache to data region
		// Note: only 1 row is stored in cache!
		// Note: row order is not preserved!
		p++;
		k1 = key, v1 = val;
		ffstr_setz(&key, p);
		ffstr_set(&val, p + key.len + 1, cache_len - key.len - 2);
		len = key.len + val.len + 2;
		cache_replace = 1;
	}

add:
	cap_cur = (c->ptr) ? *(unsigned*)c->ptr : 4;
	cap = cap_cur + 4 + 4 + size;
	if (cap > 0xffffffff)
		return CKV_E_LIMIT;
	if (!(p = ffmem_realloc(c->ptr, cap)))
		return CKV_E_NOMEM;
	c->ptr = p;

	_INT4_WRITE(p, cap);
	c->n++;

	p = c->ptr + cap_cur;
	_INT4_WRITE(p, size);
	_INT4_WRITE(p, len);

set_pair:
	p = ffmem_copy(p, key.ptr, key.len);
	*p++ = '\0';

set_val:
	p = ffmem_copy(p, val.ptr, val.len);
	*p++ = '\0';

	if (cache_replace) {
		p = c->cache;
		_INT1_WRITE(p, k1.len + v1.len + 2);

		p = ffmem_copy(p, k1.ptr, k1.len);
		*p++ = '\0';

		p = ffmem_copy(p, v1.ptr, v1.len);
		*p++ = '\0';
	}

	if (r == CKV_E_OK_CACHED && p < c->cache + sizeof(c->cache))
		_INT1_WRITE(p, 0);
	return r;
}

/**
cursor: must be initialized to 0
flags: enum CKV_LIST */
static inline int ckv_list(const struct ckv *c, unsigned *cursor, ffstr *key, ffstr *val, unsigned flags)
{
	const char *p;
	int r = CKV_E_OK;
	unsigned i = *cursor, len, size, nk, cap;

	if (i < sizeof(c->cache)) {
		p = c->cache + i;
		len = *p++;
		if (len) {
			// Note: CKV_F_UNIQUE is not checked
			i = p + len - c->cache;
			r = CKV_E_OK_CACHED;
			goto found;
		}
		i = sizeof(c->cache);
	}

	if (!c->ptr)
		return CKV_E_DONE;
	p = c->ptr;
	cap = _INT4_READ(p);

	i -= sizeof(c->cache);
	if (!i)
		i = 4;
	FF_ASSERT(i >= 4);
	p = c->ptr + i;

	for (;;) {
		if (p >= c->ptr + cap) {
			*cursor = sizeof(c->cache) + cap;
			return CKV_E_DONE;
		}

		size = _INT4_READ(p);
		len = _INT4_READ(p);
		i = sizeof(c->cache) + p + size - c->ptr;

		if (flags & CKV_F_UNIQUE) {
			if (!_ckv_find(c->ptr + 4, p - 8, FFSTR_Z(p)))
				break;
			// skip current row because same key is found before
		} else {
			break;
		}

		p += size;
	}

found:
	nk = ffsz_len(p);
	ffstr_set(key, p, nk);
	ffstr_set(val, p + nk + 1, len - nk - 2);
	*cursor = i;
	return r;
}

/**
flags: enum CKV_FIND */
static inline int ckv_find(struct ckv *c, ffstr key, ffstr *val, unsigned flags)
{
	char *p;
	unsigned len, nk;
	if ((p = _ckv_cache_find(c->cache, c->cache + sizeof(c->cache), key))) {
		len = _INT1_READ(p);
		nk = ffsz_len(p);
		ffstr_set(val, p + nk + 1, len - nk - 2);
		return CKV_E_OK_CACHED;
	}

	if (flags & CKV_F_CACHE)
		return CKV_E_NOTEXIST;

	unsigned cap_cur = (c->ptr) ? *(unsigned*)c->ptr : 4;
	if ((p = _ckv_find(c->ptr + 4, c->ptr + cap_cur, key))) {
		_INT4_READ(p);
		len = _INT4_READ(p);
		nk = ffsz_len(p);
		ffstr_set(val, p + nk + 1, len - nk - 2);
		return CKV_E_OK;
	}

	return CKV_E_NOTEXIST;
}

/**
flags: enum CKV_SET */
static inline void ckv_copy(struct ckv *dst, const struct ckv *src, unsigned flags)
{
	unsigned i = 0;
	ffstr k, v;
	while (CKV_E_DONE != ckv_list(src, &i, &k, &v, 0)) {
		ckv_set(dst, k, v, flags);
	}
}

#undef _INT4_READ
#undef _INT4_WRITE
#undef _INT1_READ
#undef _INT1_WRITE
