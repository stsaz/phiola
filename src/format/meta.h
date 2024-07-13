/** phiola: manage multimedia file meta data
2023, Simon Zolin */

#include <track.h>

static void meta_destroy(ffvec *meta)
{
	char **it;
	FFSLICE_FOR(meta, it) {
		ffmem_free(*it);
		it += 2;
	}
	ffvec_free(meta);
}

static void meta_set(ffvec *meta, ffstr name, ffstr val, uint flags)
{
	phi_dbglog(core, NULL, NULL, "meta: %S = %S", &name, &val);
	if (!name.len) return;

	char *s = ffsz_allocfmt("%S%Z%S", &name, &val);

	if (flags & PHI_META_REPLACE) {
		char **it;
		FFSLICE_FOR(meta, it) {
			if (ffstr_eqz(&name, it[0])) {
				ffmem_free(*it);
				it[0] = s;
				it[1] = s + name.len + 1;
				return;
			}
			it += 2;
		}
	}

	*ffvec_pushT(meta, char*) = s;
	*ffvec_pushT(meta, char*) = s + name.len + 1;
}

static void meta_copy(ffvec *dst, const ffvec *src)
{
	char **it;
	FFSLICE_FOR(src, it) {
		meta_set(dst, FFSTR_Z(it[0]), FFSTR_Z(it[1]), 0);
		it += 2;
	}
}

static int meta_list(const ffvec *meta, uint *i, ffstr *name, ffstr *val, uint flags)
{
	for (;;) {
		if ((*i) * 2 == meta->len)
			return 0;

		const char *k = *ffslice_itemT(meta, (*i) * 2, char*);
		const char *v = *ffslice_itemT(meta, (*i) * 2 + 1, char*);
		ffstr_setz(name, k);
		ffstr_setz(val, v);
		(*i)++;

		if (!(flags & PHI_META_PRIVATE) && ffstr_matchz(name, "_phi_"))
			continue;

		int skip = 0;
		if (flags & PHI_META_UNIQUE) {
			char **it;
			FFSLICE_FOR(meta, it) {
				if (k == *it)
					break;
				if (ffstr_ieqz(name, *it)) {
					skip = 1; // skip current k-v pair because same key is found before
					break;
				}
				it += 2;
			}
		}

		if (!skip)
			return -1;
	}
}

static int meta_find(const ffvec *meta, ffstr name, ffstr *val, uint flags)
{
	uint i = 0;
	ffstr n, v;
	while (meta_list(meta, &i, &n, &v, flags)) {
		if (ffstr_eq2(&n, &name)) {
			phi_dbglog(core, NULL, NULL, "meta requested: %S = %S", &n, &v);
			*val = v;
			return 0;
		}
	}
	return -1;
}

const phi_meta_if phi_metaif = {
	meta_set,
	meta_copy,
	meta_find,
	meta_list,
	meta_destroy,
};
