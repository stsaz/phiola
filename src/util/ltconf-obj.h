/** ltconf extension: `object {...}`
2022, Simon Zolin
*/

/*
ltconf_obj_read
ltconf_obj_fin
*/

#pragma once
#include "ltconf.h"

struct ltconf_obj {
	struct ltconf lt;
	ffvec buf;
	uint level;
};

enum {
	LTCONF_OBJ_OPEN = LTCONF_ERROR + 1,
	LTCONF_OBJ_CLOSE,
};

static inline int ltconf_obj_read(struct ltconf_obj *c, ffstr *in, ffstr *out)
{
	for (;;) {
		int r = ltconf_read(&c->lt, in, out);

		if (r == LTCONF_CHUNK || c->buf.len != 0) {
			// store new data chunk in buffer
			ffvec_add2(&c->buf, out, 1);
			ffstr_setstr(out, &c->buf);
		}

		switch (r) {
		case LTCONF_CHUNK: continue;

		case LTCONF_KEY:
			if (ffstr_eqcz(out, "}")
				&& !(c->lt.flags & LTCONF_FQUOTED)) {
				if (c->level == 0) {
					c->lt.error = "unexpected context close";
					return LTCONF_ERROR;
				}
				c->level--;
				r = LTCONF_OBJ_CLOSE;
			}
			c->buf.len = 0;
			break;

		case LTCONF_VAL:
		case LTCONF_VAL_NEXT:
			if (ffstr_eqcz(out, "{")
				&& !(c->lt.flags & LTCONF_FQUOTED)) {
				c->level++;
				r = LTCONF_OBJ_OPEN;
			}
			c->buf.len = 0;
			break;

		default: break;
		}

		return r;
	}
}

static inline int ltconf_obj_fin(struct ltconf_obj *c)
{
	ffvec_free(&c->buf);
	if (c->level != 0) {
		c->lt.error = "unclosed context";
		return -1;
	}
	return 0;
}
