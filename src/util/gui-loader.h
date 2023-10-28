/** GUI loader: cross-platform stuff.
2014, Simon Zolin */

#include <ffbase/string.h>

typedef struct ffui_ldr_ctl ffui_ldr_ctl;
struct ffui_ldr_ctl {
	const char *name;
	uint flags; //=offset
	const ffui_ldr_ctl *children;
};

#define FFUI_LDR_CTL(struct_name, ctl) \
	{ #ctl, (uint)FF_OFF(struct_name, ctl), NULL }

#define FFUI_LDR_CTL3(struct_name, ctl, children) \
	{ #ctl, (uint)FF_OFF(struct_name, ctl), children }

#define FFUI_LDR_CTL3_PTR(struct_name, ctl, children) \
	{ #ctl, 0x80000000 | (uint)FF_OFF(struct_name, ctl), children }

#define FFUI_LDR_CTL_END  {NULL, 0, NULL}

/** Find control object by its path.
name: e.g. "window.control" */
static inline void* ffui_ldr_findctl(const ffui_ldr_ctl *ctx, void *ctl, const ffstr *name)
{
	ffstr s = *name, sctl;
	while (s.len != 0) {
		ffstr_splitby(&s, '.', &sctl, &s);

		for (uint i = 0; ; i++) {
			if (ctx[i].name == NULL)
				return NULL;

			if (ffstr_eqz(&sctl, ctx[i].name)) {
				uint off = ctx[i].flags & ~0x80000000;
				ctl = (char*)ctl + off;
				if (ctx[i].flags & 0x80000000)
					ctl = *(void**)ctl;
				if (s.len == 0)
					return ctl;

				if (ctx[i].children == NULL)
					return NULL;
				ctx = ctx[i].children;
				break;
			}
		}
	}
	return NULL;
}
