/** phiola: playlist entry
2023, Simon Zolin */

#include <FFOS/path.h>
#include <ffbase/vector.h>

typedef struct pls_entry {
	ffvec url;
	ffvec artist;
	ffvec title;
	int duration;
	uint clear :1;
} pls_entry;

static inline void pls_entry_free(pls_entry *ent)
{
	ffvec_free(&ent->url);
	ffvec_free(&ent->artist);
	ffvec_free(&ent->title);
	ent->duration = -1;
}

/** Parse URI scheme.
Return scheme length on success. */
static inline uint ffuri_scheme(ffstr name)
{
	ffstr scheme;
	if (ffstr_matchfmt(&name, "%S://", &scheme) <= 0)
		return 0;
	return scheme.len;
}

/** Get absolute filename. */
static inline int plist_fullname(phi_track *t, ffstr name, ffstr *dst)
{
	const char *fn;
	ffstr path = {};

	if (!ffpath_abs(name.ptr, name.len)
		&& 0 == ffuri_scheme(name)) {

		fn = t->conf.ifile.name;
		if (0 != ffpath_splitpath_str(FFSTR_Z(fn), &path, NULL))
			path.len++;
	}

	char *s = ffsz_allocfmt("%S%S", &path, &name);
	ffstr_setz(dst, s);
	return 0;
}
