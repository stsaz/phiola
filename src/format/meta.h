/** phiola: manage multimedia file meta data
2023, Simon Zolin */

#include <track.h>

static void meta_destroy(phi_meta *meta)
{
	char **it;
	FFSLICE_FOR(meta, it) {
		ffmem_free(*it);
		it += 2;
	}
	ffmem_free(meta->ptr);
	phi_meta_null(meta);
}

static void meta_set(phi_meta *meta, ffstr name, ffstr val, uint flags)
{
	phi_dbglog(core, NULL, NULL, "meta: %S = %S", &name, &val);
	if (!name.len) return;

	if (flags & PHI_META_UNIQUE) {
		char **it;
		FFSLICE_FOR(meta, it) {
			if (ffstr_ieqz(&name, it[0]))
				return;
			it += 2;
		}
	}

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

	if (meta->len + 2 > meta->cap) {
		meta->cap = meta->len + ffmax(2, meta->len / 2);
		meta->ptr = ffmem_realloc(meta->ptr, meta->cap * sizeof(char*));
	}
	char **m = meta->ptr;
	m[meta->len] = s;
	m[meta->len + 1] = s + name.len + 1;
	meta->len += 2;
}

static void meta_copy(phi_meta *dst, const phi_meta *src, uint flags)
{
	char **it;
	FFSLICE_FOR(src, it) {
		meta_set(dst, FFSTR_Z(it[0]), FFSTR_Z(it[1]), flags);
		it += 2;
	}
}

static int meta_list(const phi_meta *meta, uint *index, ffstr *name, ffstr *val, uint flags)
{
	int r = 1;
	uint i = *index;
	const char **m = meta->ptr;
	for (;;) {
		if (i * 2 == meta->len) {
			r = 0;
			break;
		}

		const char *k = m[i * 2], *v = m[i * 2 + 1];
		ffstr_setz(name, k);
		ffstr_setz(val, v);
		i++;

		if (!(flags & PHI_META_PRIVATE) && ffstr_matchz(name, "_phi_"))
			continue;

		int skip = 0;
		if (flags & PHI_META_UNIQUE) {
			for (const char **it = meta->ptr;  *it != k;  it += 2) {
				if (ffstr_ieqz(name, *it)) {
					skip = 1; // skip current k-v pair because same key is found before
					break;
				}
			}
		}
		if (!skip)
			break;
	}

	*index = i;
	return r;
}

static int meta_find(const phi_meta *meta, ffstr name, ffstr *val, uint flags)
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
