/** Light config parser
2022, Simon Zolin
*/

/*
Format:

	[SPACE] KEY [SPACE VALUE]... [SPACE #COMMENT] LF
	[SPACE #COMMENT] LF

SPACE:
	{TAB, LF, CR, SPC} characters

KEY, VALUE:
	* text
		all printable except '"'
	* " quoted-text "
		all printable plus {TAB, LF, CR, SPC} except '\'
		Supported escape sequences: \\, \", \xHH (HH: two hexadecimal digits)

#COMMENT:
	text starting with '#'
*/

/*
Synthetic benchmark (i5-8250U, clang-16, 50MB "fmedia.gui"):

	                 MB/sec
	=======================
	ltconf sse42 -O3  405
	ltconf -O3/-O2    298
	conf2* -O3        210
	ltconf sse42 -O0   99
	ltconf -O0         69
	conf2 -O0          65

	* 'conf2' is a straightforward implementation with a per-byte reading loop
*/

#pragma once
#include <ffbase/string.h>

struct ltconf {
	ffuint state;
	ffuint line, line_off;
	ffuint flags; // enum LTCONF_FLAGS + enum LTCONF_OPT
	char xdata;
	const char *error;
};

enum LTCONF_R {
	/** Need more input data */
	LTCONF_MORE,

	/** Output contains unfinished data chunk.
	Check if it's key (and not value) with `!(ltconf.flags & LTCONF_FKEY)`. */
	LTCONF_CHUNK,

	/** The first word on a new line */
	LTCONF_KEY,

	/** First word after the key
	Check if data is quoted with `!!(ltconf.flags & LTCONF_FQUOTED)`. */
	LTCONF_VAL,

	/** Second or next value */
	LTCONF_VAL_NEXT,

	/** Reached an invalid character.
	ltconf.error contains error description. */
	LTCONF_ERROR,
};

enum LTCONF_FLAGS {
	LTCONF_FKEY = 1,
	LTCONF_FVAL = 2,
	LTCONF_FQUOTED = 4,
	LTCONF_FCHUNKED = 8,
};

enum LTCONF_OPT {
	LTCONF_OPT_DISABLE_COMMENTS = 0x0100,
};

/* Algorithm:
white:
	'\n': line++
	'#': -> comment
	skip " \t\r\n"
	-> word

comment:
	'\n': -> white
	SKIP

word:
	'"': -> quote
	skip printable: -> text
	CHUNK

quote:
	'"': -> text
	'\\': -> backslash
	'\n': line++
	CHUNK

backslash:
	CHUNK
	-> quote

text:
	KEY|VAL
	-> white1

white1:
	-> white
*/

/** Read and consume input data and set key/value output data.
Return enum LTCONF_R. */
static inline int ltconf_read(struct ltconf *c, ffstr *in, ffstr *out)
{
	enum { I_WHITE, I_COMMENT, I_WORD, I_QUOTE,
		I_BACKSLASH, I_BACKSLASH_X1, I_BACKSLASH_X2,
		I_WHITE1, };
	const char *d = in->ptr, *end = in->ptr + in->len;
	const char *line_start = d - c->line_off;
	int r;

	for (;;) {
		if (d == end)
			goto skip;

		switch (c->state) {

		case I_WHITE1:
			if (!(d[0] == ' ' || d[0] == '\t' || d[0] == '\r' || d[0] == '\n')) {
				c->error = "require whitespace after word or quoted text";
				goto err;
			}
			c->flags &= ~LTCONF_FQUOTED;
			c->state = I_WHITE;
			// fallthrough

		case I_WHITE:
			d += ffs_skipany(d, end - d, " \t\r\r", 4);
			if (d == end)
				goto skip;

			if (d[0] == '\n') {
				d++;
				c->flags &= ~0x0f;
				c->line++;
				c->line_off = 0;
				line_start = d;
				continue;
			} else if (d[0] == '#' && !(c->flags & LTCONF_OPT_DISABLE_COMMENTS)) {
				c->state = I_COMMENT;
				continue;
			}

			ffstr_set(in, d, end - d);
			c->state = I_WORD;
			continue;

		case I_COMMENT:
			if (NULL == (d = (char*)ffmem_findbyte(d, end - d, '\n')))
				goto skip;
			c->state = I_WHITE;
			continue;

		case I_WORD:
			if (d[0] == '"') {
				d++;
				ffstr_set(in, d, end - d);
				c->flags |= LTCONF_FQUOTED;
				c->state = I_QUOTE;
				continue;
			}

			// all printable except '"'(22)
			r = ffs_skip_ranges(d, end - d, "\x21\x21\x23\x7e\x80\xff", 6);
			if (r < 0) {
				if (d == end)
					goto skip;
				ffstr_set(out, d, end - d);
				d = end;
				goto chunk;
			} else if (r == 0 && !(c->flags & LTCONF_FCHUNKED)) {
				c->error = "non-printable character or a quote";
				goto err;
			}

			ffstr_set(out, d, r);
			d += r;
			c->state = I_WHITE1;
			goto text;

		case I_QUOTE:
			// all printable except TAB(9), CR(d), SPC(20), '"'(22), '\'(5c)
			r = ffs_skip_ranges(d, end - d, "\x09\x09\x0d\x0d\x20\x21\x23\x5b\x5d\x7e\x80\xff\xff\xff\xff\xff", 16);
			if (r < 0) {
				ffstr_set(out, d, end - d);
				d = end;
				goto chunk;
			}

			switch (d[r]) {
			case '\n':
				d += r+1;
				c->line++;
				c->line_off = 0;
				line_start = d;
				continue;

			case '\\':
				ffstr_set(out, in->ptr, d+r - in->ptr);
				d += r+1;
				c->state = I_BACKSLASH;
				if (r == 0)
					continue;
				goto chunk;

			case '"':
				ffstr_set(out, in->ptr, d+r - in->ptr);
				d += r+1;
				c->state = I_WHITE1;
				goto text;
			}

			d += r;
			c->error = "non-printable character within quotes";
			goto err;

		case I_BACKSLASH:
			if (d[0] == '"' || d[0] == '\\') {
				ffstr_set(out, d, 1);
			} else if (d[0] == 'x') {
				d++;
				c->state = I_BACKSLASH_X1;
				continue;
			} else {
				c->error = "bad escape sequence";
				goto err;
			}
			d++;
			c->state = I_QUOTE;
			goto chunk;

		case I_BACKSLASH_X1:
			c->xdata = d[0];
			d++;
			c->state = I_BACKSLASH_X2;
			// fallthrough

		case I_BACKSLASH_X2: {
			static const char char_to_hex[256] = {
				-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
				-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,-1,-1,-1,-1,-1,-1,
				-1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
				-1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
				-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
				-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
				-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
				-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
			};
			int hi = char_to_hex[(ffbyte)(c->xdata & 0xff)];
			int lo = char_to_hex[(ffbyte)(d[0] & 0xff)];
			if (hi < 0 || lo < 0) {
				c->error = "bad x-byte sequence";
				goto err;
			}
			c->xdata = (hi<<4) | lo;
			d++;
			ffstr_set(out, &c->xdata, 1);
			c->state = I_QUOTE;
			goto chunk;
		}

		default:
			FF_ASSERT(0);
			c->error = "memory corruption";
			goto err;
		}
	}

skip:
	c->line_off = d - line_start;
	out->len = 0;
	in->len = 0;
	return LTCONF_MORE;

chunk:
	c->line_off = d - line_start;
	ffstr_set(in, d, end - d);
	c->flags |= LTCONF_FCHUNKED;
	return LTCONF_CHUNK;

text:
	c->line_off = d - line_start;
	ffstr_set(in, d, end - d);
	c->flags &= ~LTCONF_FCHUNKED;
	if (!(c->flags & LTCONF_FKEY)) {
		c->flags |= LTCONF_FKEY;
		return LTCONF_KEY;
	}
	if (!(c->flags & LTCONF_FVAL)) {
		c->flags |= LTCONF_FVAL;
		return LTCONF_VAL;
	}
	return LTCONF_VAL_NEXT;

err:
	c->line_off = d - line_start;
	return LTCONF_ERROR;
}

/** Return current line # starting at 1 */
#define ltconf_line(c)  ((c)->line+1)

/** Return current column # starting at 1 */
#define ltconf_col(c)  ((c)->line_off+1)

/** Get error message after LTCONF_ERROR */
#define ltconf_error(c)  ((c)->error)
