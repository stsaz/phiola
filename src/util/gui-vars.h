/** phiola: GUI variables
2022, Simon Zolin */

#include <ffbase/map.h>
#include "ltconf.h"

struct guivar {
	ffstr name, val;
};

static inline int _vars_keyeq(void *opaque, const void *key, ffsize keylen, void *val)
{
	const struct guivar *v = (struct guivar*)val;
	return 0 == ffstr_cmp(&v->name, (char*)key, keylen);
}

static inline void vars_init(ffmap *vs)
{
	ffmap_init(vs, _vars_keyeq);
}

static inline void vars_free(ffmap *vs)
{
	struct _ffmap_item *it;
	FFMAP_WALK(vs, it) {
		if (!_ffmap_item_occupied(it))
			continue;
		struct guivar *v = (struct guivar*)it->val;
		ffmem_free(v);
	}
	ffmap_free(vs);
}

/** Find string by its name */
static inline ffstr vars_val(ffmap *vs, ffstr name)
{
	ffstr s = name;
	if (name.len != 0 && name.ptr[0] == '$') {
		ffstr_shift(&name, 1);
		struct guivar *v;
		if (NULL != (v = (struct guivar*)ffmap_find(vs, name.ptr, name.len, NULL))) {
			s = v->val;
		}
	}
	return s;
}

static inline void vars_set(ffmap *vs, ffstr name, ffstr val)
{
	struct guivar *v;
	int nu = 0;
	if (NULL == (v = (struct guivar*)ffmap_find(vs, name.ptr, name.len, NULL))) {
		v = ffmem_new(struct guivar);
		nu = 1;
	}

	ffstr_setstr(&v->name, &name);
	ffstr_setstr(&v->val, &val);
	if (nu)
		ffmap_add(vs, name.ptr, name.len, v);
}

/** Load variables from data:
(NAME "VAL" \n)...
*/
static inline int vars_load(ffmap *vs, ffstr data)
{
	struct ltconf c = {};
	ffstr s, key = {};
	while (data.len) {
		int r = ltconf_read(&c, &data, &s);
		switch (r) {
		case LTCONF_MORE: break;

		case LTCONF_KEY:
			key = s;  break;

		case LTCONF_VAL:
			vars_set(vs, key, s);
			break;

		default:
			return r;
		}
	}

	return 0;
}
