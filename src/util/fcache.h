/** fcom: file cache
2022, Simon Zolin */

/*
fcache_init
fcache_destroy
fcache_reset
fcache_nextbuf
fcache_find
*/

#pragma once
#include <ffbase/slice.h>

// can cast to ffstr*
struct fcache_buf {
	ffsize len;
	char *ptr;
	ffuint64 off;
};

struct fcache {
	ffslice bufs; // struct fcache_buf[]
	ffuint idx;
	struct {
		ffuint64 hits, misses;
	};
};

static inline int fcache_init(struct fcache *c, ffuint nbufs, ffuint bufsize, ffuint align)
{
	if (NULL == ffslice_zallocT(&c->bufs, nbufs, struct fcache_buf))
		return 1;
	c->bufs.len = nbufs;

	struct fcache_buf *b;
	FFSLICE_WALK(&c->bufs, b) {
		if (NULL == (b->ptr = ffmem_align(bufsize, align)))
			return 1;
		b->off = (ffuint64)-1;
	}
	return 0;
}

static inline void fcache_destroy(struct fcache *c)
{
	struct fcache_buf *b;
	FFSLICE_WALK(&c->bufs, b) {
		ffmem_alignfree(b->ptr);
	}
	ffslice_free(&c->bufs);
}

static inline void fcache_reset(struct fcache *c)
{
	struct fcache_buf *b;
	FFSLICE_WALK(&c->bufs, b) {
		b->off = 0;
		b->len = 0;
	}
}

static inline struct fcache_buf* fcache_nextbuf(struct fcache *c)
{
	struct fcache_buf *b = c->bufs.ptr;
	b = &b[c->idx];
	c->idx = (c->idx + 1) % c->bufs.len;
	return b;
}

/** Find cached buffer with data at `off`. */
static inline struct fcache_buf* fcache_find(struct fcache *c, ffuint64 off)
{
	struct fcache_buf *b;
	FFSLICE_WALK(&c->bufs, b) {
		if (off >= b->off  &&  off < b->off + b->len) {
			c->hits++;
			return b;
		}
	}
	c->misses++;
	return NULL;
}

/** Write data into buffer
Return >=0: output file offset
  <0: no output data */
static inline ffint64 fbuf_write(struct fcache_buf *b, ffsize cap, ffstr *in, uint64 off, ffstr *out)
{
	if (in->len == 0)
		return -1;

	if (b->len != 0) {
		if (off >= b->off  &&  off <= b->off + b->len  &&  off < b->off + cap) {
			// new data overlaps with our buffer
			off -= b->off;
			uint64 n = ffmin(in->len, cap - off);
			ffmem_copy(b->ptr + off, in->ptr, n);
			ffstr_shift(in, n);
			if (b->len < off + n) {
				b->len = off + n;
				if (b->len != cap)
					return -1;
			}
		}

		// flush bufferred data
		ffstr_set(out, b->ptr, b->len);
		return b->off;
	}

	if (cap < in->len) {
		// input data is very large, don't buffer it
		*out = *in;
		ffstr_shift(in, in->len);
		return off;
	}

	// store input data
	uint64 n = in->len;
	ffmem_copy(b->ptr, in->ptr, n);
	ffstr_shift(in, n);
	b->len = n;
	b->off = off;
	return -1;
}
