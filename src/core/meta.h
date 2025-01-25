/** phiola: manage multimedia file meta data
2023, Simon Zolin */

struct _phi_meta {
	u_char len, cap; // N of pointers in `ptr`
	u_char ndata; // N of bytes in `data`
	char data[64 - 3];
	char *ptr[0]; // ([0]=name, [1]=value)...
};

#define META_SELF_CONTAINED(m, p) \
	(p >= m->data && p < m->data + sizeof(m->data))

static void meta_destroy(phi_meta *meta)
{
	struct _phi_meta *m = *meta;
	if (!m) return;

	for (uint i = 0;  i < m->len;  i += 2) {
		if (!META_SELF_CONTAINED(m, m->ptr[i]))
			ffmem_free(m->ptr[i]);
	}
	// dbglog("meta: %u/%u  ndata:%u", m->len, m->cap, m->ndata);
	ffmem_free(m);
	*meta = NULL;
}

static void meta_set(phi_meta *meta, ffstr name, ffstr val, uint flags)
{
	struct _phi_meta *m = *meta;
	dbglog("meta: %S = %S", &name, &val);
	if (!name.len) return;
	uint n = 0, cap = 0;
	char *s;

	if (m && (flags & PHI_META_UNIQUE)) {
		for (uint i = 0;  i < m->len;  i += 2) {
			if (ffstr_ieqz(&name, m->ptr[i]))
				return;
		}
	}

	if (m && (flags & PHI_META_REPLACE)) {
		for (uint i = 0;  i < m->len;  i += 2) {
			if (ffstr_eqz(&name, m->ptr[i])) {
				if (!META_SELF_CONTAINED(m, m->ptr[i]))
					ffmem_free(m->ptr[i]);
				n = i;
				goto fin;
			}
		}
	}

	if (!m) {
		cap = 2;
	} else {
		n = m->len;
		if (n + 2 > m->cap)
			cap = n + ffmax(2, n / 2);
		if (ff_unlikely(cap > 0xff)) {
			errlog("meta tags limit reached");
			return;
		}
	}
	if (cap) {
		m = ffmem_realloc(m, sizeof(struct _phi_meta) + cap * sizeof(char*));
		if (n == 0) {
			ffmem_zero_obj(m);
		} else {
			const struct _phi_meta *m_old = *meta;
			ssize_t off = (char*)m - (char*)m_old;
			for (uint i = 0;  i < m->len;  i += 2) {
				if (META_SELF_CONTAINED(m_old, m->ptr[i])) {
					m->ptr[i] += off;
					m->ptr[i + 1] += off;
				}
			}
		}
		*meta = m;
		m->cap = cap;
	}
	m->len = n + 2;

fin:
	cap = name.len + val.len + 2;
	if (cap > sizeof(m->data) - m->ndata) {
		s = ffmem_alloc(cap);
	} else {
		s = m->data + m->ndata;
		m->ndata += cap;
	}
	ffs_format(s, -1, "%S%Z%S%Z", &name, &val);
	m->ptr[n] = s;
	m->ptr[n + 1] = s + name.len + 1;
}

static void meta_copy(phi_meta *dst, const phi_meta *src, uint flags)
{
	struct _phi_meta *m = *src;
	if (!m) return;

	for (uint i = 0;  i < m->len;  i += 2) {
		meta_set(dst, FFSTR_Z(m->ptr[i]), FFSTR_Z(m->ptr[i + 1]), flags);
	}
}

static int meta_list(const phi_meta *meta, uint *index, ffstr *name, ffstr *val, uint flags)
{
	struct _phi_meta *m = *meta;
	if (!m) return 0;

	int r = 1;
	uint i = *index * 2;
	for (;;) {
		if (i >= m->len) {
			r = 0;
			break;
		}

		ffstr_setz(name, m->ptr[i]);
		ffstr_setz(val, m->ptr[i + 1]);
		i += 2;

		if (!(flags & PHI_META_PRIVATE) && ffstr_matchz(name, "_phi_"))
			continue;

		int skip = 0;
		if (flags & PHI_META_UNIQUE) {
			for (uint j = 0;  j < i - 2;  j += 2) {
				if (ffstr_ieqz(name, m->ptr[j])) {
					skip = 1; // skip current k-v pair because same key is found before
					break;
				}
			}
		}
		if (!skip)
			break;
	}

	*index = i / 2;
	return r;
}

static int meta_find(const phi_meta *meta, ffstr name, ffstr *val, uint flags)
{
	if (!*meta) return -1;

	uint i = 0;
	ffstr n, v;
	while (meta_list(meta, &i, &n, &v, flags)) {
		if (ffstr_ieq2(&n, &name)) {
			dbglog("meta requested: %S = %S", &n, &v);
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
