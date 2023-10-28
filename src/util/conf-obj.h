/** ffconf extension: `object {...}`; partial input
2022, Simon Zolin */

/*
ffconf_obj_read
ffconf_obj_fin
*/

#pragma once
#include <ffbase/conf.h>
#include <ffbase/vector.h>

struct ffconf_obj {
	struct ffconf lt;
	ffvec buf;
	uint level;
};

enum {
	FFCONF_OBJ_OPEN = FFCONF_ERROR + 1,
	FFCONF_OBJ_CLOSE,
};

static inline int ffconf_obj_read(struct ffconf_obj *c, ffstr *in, ffstr *out)
{
	for (;;) {
		int r = ffconf_read(&c->lt, in, out);

		if (r == FFCONF_CHUNK || c->buf.len != 0) {
			// store new data chunk in buffer
			ffvec_add2(&c->buf, out, 1);
			ffstr_setstr(out, &c->buf);
		}

		switch (r) {
		case FFCONF_CHUNK: continue;

		case FFCONF_KEY:
			if (ffstr_eqcz(out, "}")
				&& !(c->lt.flags & FFCONF_FQUOTED)) {
				if (c->level == 0) {
					c->lt.error = "unexpected context close";
					return FFCONF_ERROR;
				}
				c->level--;
				r = FFCONF_OBJ_CLOSE;
			}
			c->buf.len = 0;
			break;

		case FFCONF_VAL:
		case FFCONF_VAL_NEXT:
			if (ffstr_eqcz(out, "{")
				&& !(c->lt.flags & FFCONF_FQUOTED)) {
				c->level++;
				r = FFCONF_OBJ_OPEN;
			}
			c->buf.len = 0;
			break;

		default: break;
		}

		return r;
	}
}

static inline int ffconf_obj_fin(struct ffconf_obj *c)
{
	ffvec_free(&c->buf);
	if (c->level != 0) {
		c->lt.error = "unclosed context";
		return -1;
	}
	return 0;
}
