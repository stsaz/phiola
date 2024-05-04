/** fcom: file cache
2022, Simon Zolin */

/*
fcache_init fcache_destroy
fcache_reset
fcache_curbuf fcache_nextbuf
fcache_find
fbuf_write
fbuf_str
*/

#pragma once
#include <ffbase/string.h>

// can cast to ffstr*
struct fcache_buf {
	ffsize len;
	char *ptr;
	ffuint64 off;
};

struct fcache {
	struct fcache_buf bufs[4];
	ffuint n, idx;
	struct {
		ffuint64 hits, misses;
	};
};

static inline int fcache_init(struct fcache *c, ffuint nbufs, ffuint bufsize, ffuint align)
{
	FF_ASSERT(nbufs <= FF_COUNT(c->bufs));

	char *p = ffmem_align(bufsize * nbufs, align);
	if (!p)
		return 1;

	c->n = nbufs;
	for (uint i = 0;  i < c->n;  i++) {
		struct fcache_buf *b = &c->bufs[i];
		b->len = 0;
		b->off = 0;
		b->ptr = p;
		p += bufsize;
	}
	return 0;
}

static inline void fcache_destroy(struct fcache *c)
{
	ffmem_alignfree(c->bufs[0].ptr);
}

static inline void fcache_reset(struct fcache *c)
{
	for (uint i = 0;  i < c->n;  i++) {
		struct fcache_buf *b = &c->bufs[i];
		b->off = 0;
		b->len = 0;
	}
}

static inline struct fcache_buf* fcache_curbuf(struct fcache *c)
{
	return &c->bufs[c->idx];
}

static inline struct fcache_buf* fcache_nextbuf(struct fcache *c)
{
	struct fcache_buf *b = &c->bufs[c->idx];
	c->idx = (c->idx + 1) % c->n;
	return b;
}

/** Find cached buffer with data at `off`. */
static inline struct fcache_buf* fcache_find(struct fcache *c, ffuint64 off)
{
	for (uint i = 0;  i < c->n;  i++) {
		struct fcache_buf *b = &c->bufs[i];
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

static inline ffstr fbuf_str(struct fcache_buf *b) { return *(ffstr*)b; }
