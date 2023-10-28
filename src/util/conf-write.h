/** ffbase: conf writer
2021, Simon Zolin */

/*
ffconfw_init ffconfw_close
ffconfw_add2 ffconfw_add2z ffconfw_add2u ffconfw_add2obj
ffconfw_add_key ffconfw_add_keyz
ffconfw_add_str ffconfw_add_strz
ffconfw_add_int ffconfw_add_intf
ffconfw_add_float
ffconfw_add_line ffconfw_add_linez
ffconfw_add_obj
ffconfw_add
ffconfw_fin
ffconfw_clear
*/

#pragma once
#include "conf-scheme.h"

typedef struct ffconfw {
	ffvec buf;
	ffuint level;
	ffuint flags;
} ffconfw;

enum FFCONFW_FLAGS {
	/** Don't escape strings */
	FFCONFW_FDONTESCAPE = 1<<30,

	/** Add value as is, no quotes */
	FFCONFW_FLINE = 1<<29,

	/** Use CRLF instead of LF */
	FFCONFW_FCRLF = 1<<28,

	/** Use TAB instead of SPC for key-value delimiter */
	FFCONFW_FKVTAB = 1<<27,

	/** Indent with TABs */
	FFCONFW_FINDENT = 1<<26,
};

/** Initialize writer
flags: enum FFCONFW_FLAGS */
static inline void ffconfw_init(ffconfw *c, ffuint flags)
{
	ffmem_zero_obj(c);
	c->flags = flags;
}

/** Close writer */
static inline void ffconfw_close(ffconfw *c)
{
	ffvec_free(&c->buf);
}

static ffsize ffconf_escape(char *dst, ffsize cap, const char *s, ffsize len)
{
	static const char esc_char[256] = {
		1,1,  1,1,1,1,1,1,'b','t','n',1,'f','r',1,1,1,1,1,1,1,1,1,1,1,1,1,1,   1,1,1,1,
		0,0,'"',0,0,0,0,0,  0,  0,  0,0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   0,0,0,0,
		0,0,  0,0,0,0,0,0,  0,  0,  0,0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'\\',0,0,0,
		0,0,  0,0,0,0,0,0,  0,  0,  0,0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   0,0,0,1,
		0,//...
	};
	ffsize n = 0;

	if (dst == NULL) {
		for (ffsize i = 0;  i < len;  i++) {
			ffuint ch = esc_char[(ffbyte)s[i]];
			if (ch == 0)
				n++;
			else if (ch == 1)
				n += FFS_LEN("\\x??");
			else
				n += FFS_LEN("\\?");
		}
		return n;
	}

	for (ffsize i = 0;  i < len;  i++) {
		ffuint ch = esc_char[(ffbyte)s[i]];

		if (ch == 0) {
			dst[n++] = s[i];

		} else if (ch == 1) {
			if (n + FFS_LEN("\\x??") > cap)
				return 0;
			dst[n++] = '\\';
			dst[n++] = 'x';
			dst[n++] = ffHEX[(ffbyte)s[i] >> 4];
			dst[n++] = ffHEX[(ffbyte)s[i] & 0x0f];

		} else {
			if (n + FFS_LEN("\\?") > cap)
				return 0;

			dst[n++] = '\\';
			dst[n++] = ch;
		}
	}

	return n;
}

static inline int ffconfw_size(ffconfw *c, ffuint type_flags, const void *src, int *complex)
{
	ffsize cap = 0;
	*complex = 0;

	switch (type_flags & 0x8000000f) {

	case FFCONF_TSTR:
	case 1U<<31: {
		ffstr s = *(ffstr*)src;
		ffsize r = s.len;
		if (!(type_flags & FFCONFW_FDONTESCAPE))
			r = ffconf_escape(NULL, 0, s.ptr, s.len);
		const char *MUST_QUOTE = " {}#/\\";
		if (r != s.len || s.len == 0 || ffstr_findanyz(&s, MUST_QUOTE) >= 0)
			*complex = 1;
		cap = FFS_LEN("\r\n\"\"") + r;
		break;
	}

	case _FFCONF_TINT:
		cap = 1 + FFS_INTCAP;
		break;

	case FFCONF_TOBJ | (1U<<31):
	case FFCONF_TOBJ:
		cap = FFS_LEN("\r\n}");
		break;
	}
	return cap + c->level;
}

/** Add 1 JSON element
Reallocate buffer by twice the size
type_flags: enum FFCONFW_FLAGS
src: ffint64 | ffstr
Return N of bytes written;
 <0 on error */
static inline int ffconfw_add(ffconfw *c, ffuint type_flags, const void *src)
{
	int t = type_flags & 0x8000000f;
	int complex;
	type_flags |= c->flags;
	int kv_delim = (type_flags & FFCONFW_FKVTAB) ? '\t' : ' ';
	ffsize r = ffconfw_size(c, type_flags, src, &complex);
	if (NULL == ffvec_growtwice(&c->buf, r, 1))
		return -1;
	ffsize old_len = c->buf.len;

	switch (t) {

	case FFCONF_TSTR:
	case 1<<31: {
		ffstr s = *(ffstr*)src;

		if (t == FFCONF_TSTR) {
			// key1 (+ val1)
			// key1 val1 (+ val2)
			*ffstr_push(&c->buf) = kv_delim;

		} else if (c->buf.len != 0) {
			// key1 val1
			// (+CRLF [\t] key2)
			if (type_flags & FFCONFW_FCRLF)
				*ffstr_push(&c->buf) = '\r';
			*ffstr_push(&c->buf) = '\n';

			for (uint i = 0;  i < c->level;  i++) {
				*ffstr_push(&c->buf) = '\t';
			}
		}

		if (complex && !(type_flags & FFCONFW_FLINE)) {
			*ffstr_push(&c->buf) = '\"';
			if (!(type_flags & FFCONFW_FDONTESCAPE))
				c->buf.len += ffconf_escape(ffstr_end(&c->buf), ffvec_unused(&c->buf), s.ptr, s.len);
			else
				ffstr_add((ffstr*)&c->buf, c->buf.cap, s.ptr, s.len);
			*ffstr_push(&c->buf) = '\"';

		} else {
			ffstr_add((ffstr*)&c->buf, c->buf.cap, s.ptr, s.len);
		}
		break;
	}

	case _FFCONF_TINT: {
		ffint64 i = *(ffint64*)src;
		// key1 (+ 1234)
		*ffstr_push(&c->buf) = kv_delim;
		c->buf.len += ffs_fromint(i, ffstr_end(&c->buf), ffvec_unused(&c->buf), FFS_INTSIGN);
		break;
	}

	case FFCONF_TOBJ | (1<<31):
		// key {
		*ffstr_push(&c->buf) = ' ';
		*ffstr_push(&c->buf) = '{';
		if (type_flags & FFCONFW_FINDENT)
			c->level++;
		break;

	case FFCONF_TOBJ:
		// [\t] }
		if (type_flags & FFCONFW_FINDENT) {
			FF_ASSERT(c->level > 0);
			c->level--;
		}
		for (uint i = 0;  i < c->level;  i++) {
			*ffstr_push(&c->buf) = '\t';
		}

		if (type_flags & FFCONFW_FCRLF)
			*ffstr_push(&c->buf) = '\r';
		*ffstr_push(&c->buf) = '\n';
		*ffstr_push(&c->buf) = '}';
		break;
	}

	return c->buf.len - old_len;
}

static inline int ffconfw_addf(ffconfw *c, const char *format, ...)
{
	ffsize old_len = c->buf.len;
	if (NULL == ffvec_growtwice(&c->buf, c->level + 2, 1))
		return -1;

	if (c->flags & FFCONFW_FCRLF)
		*ffstr_push(&c->buf) = '\r';
	*ffstr_push(&c->buf) = '\n';

	for (uint i = 0;  i < c->level;  i++) {
		*ffstr_push(&c->buf) = '\t';
	}

	va_list va;
	va_start(va, format);
	ffvec_addfmtv(&c->buf, format, va);
	va_end(va);

	return c->buf.len - old_len;
}

/** Add line */
static inline int ffconfw_add_line(ffconfw *c, ffstr s)
{
	return ffconfw_add(c, (1<<31) | FFCONFW_FLINE | FFCONFW_FDONTESCAPE, &s);
}

/** Add line */
static inline int ffconfw_add_linez(ffconfw *c, const char *sz)
{
	ffstr s = FFSTR_INITZ(sz);
	return ffconfw_add(c, (1<<31) | FFCONFW_FLINE | FFCONFW_FDONTESCAPE, &s);
}

/** Add string */
static inline int ffconfw_add_str(ffconfw *c, ffstr s)
{
	return ffconfw_add(c, FFCONF_TSTR, &s);
}

/** Add NULL-terminated string */
static inline int ffconfw_add_strz(ffconfw *c, const char *sz)
{
	ffstr s;
	ffstr_setz(&s, sz);
	return ffconfw_add(c, FFCONF_TSTR, &s);
}

/** Add NULL-terminated string */
static inline int ffconfw_add_key(ffconfw *c, ffstr s)
{
	return ffconfw_add(c, 1<<31, &s);
}

/** Add NULL-terminated string */
static inline int ffconfw_add_keyz(ffconfw *c, const char *sz)
{
	ffstr s = FFSTR_INITZ(sz);
	return ffconfw_add(c, 1<<31, &s);
}

/** Add integer */
static inline int ffconfw_add_intf(ffconfw *c, ffint64 val, ffuint int_flags)
{
	char buf[64];
	int r = ffs_fromint(val, buf, sizeof(buf), FFS_INTSIGN | int_flags);
	ffstr s;
	ffstr_set(&s, buf, r);
	return ffconfw_add(c, FFCONF_TSTR, &s);
}

/** Add integer */
static inline int ffconfw_add_int(ffconfw *c, ffint64 val)
{
	return ffconfw_add_intf(c, val, 0);
}

/**
float_flags: precision | enum FFS_FROMFLOAT | FFS_FLTWIDTH() */
static inline ffsize ffconfw_add_float(ffconfw *c, double val, ffuint float_flags)
{
	char buf[64];
	uint n = ffs_fromfloat(val, buf, sizeof(buf), float_flags);
	ffstr s;
	ffstr_set(&s, buf, n);
	return ffconfw_add(c, FFCONF_TSTR, &s);
}

/** Add object */
static inline int ffconfw_add_obj(ffconfw *c, ffuint open)
{
	return ffconfw_add(c, FFCONF_TOBJ | ((open) ? 1<<31 : 0), NULL);
}

/** Add key and value */
static inline int ffconfw_add2(ffconfw *c, ffstr key, ffstr val)
{
	int n, r;
	if ((n = ffconfw_add_key(c, key)) < 0)
		return n;
	if ((r = ffconfw_add(c, FFCONF_TSTR, &val)) < 0)
		return r;
	return n + r;
}

/** Add key and value */
static inline int ffconfw_add2s(ffconfw *c, const char *key, ffstr val)
{
	return ffconfw_add2(c, FFSTR_Z(key), val);
}

/** Add key and value as NULL-terminated string */
static inline int ffconfw_add2z(ffconfw *c, const char *key, const char *val)
{
	return ffconfw_add2(c, FFSTR_Z(key), FFSTR_Z(val));
}

static inline int ffconfw_add2u(ffconfw *c, const char *key, ffuint64 val)
{
	int n, r;
	if ((n = ffconfw_add_keyz(c, key)) < 0)
		return n;
	if ((r = ffconfw_add_int(c, val)) < 0)
		return r;
	return n + r;
}

static inline int ffconfw_add2obj(ffconfw *c, const char *key, ffuint obj_open)
{
	int n, r;
	if ((n = ffconfw_add_keyz(c, key)) < 0)
		return n;
	if ((r = ffconfw_add_obj(c, obj_open)) < 0)
		return r;
	return n + r;
}

static inline int ffconfw_fin(ffconfw *c)
{
	ffvec_addchar(&c->buf, '\n');
	return 0;
}

#define ffconfw_clear(c)  ((c)->buf.len = 0)
