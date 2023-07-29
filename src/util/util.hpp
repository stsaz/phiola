/** phiola: C++ utility functions
2023, Simon Zolin */

#include <ffbase/string.h>

struct ffstrxx : ffstr {
	ffstrxx() { ptr = NULL;  len = 0; }
	ffstrxx(ffstr s) { ptr = s.ptr;  len = s.len; }
	bool operator==(const char *sz) const { return ffstr_eqz(this, sz); }
	ffssize split(char by, ffstrxx *left, ffstrxx *right) const { return ffstr_splitby(this, by, left, right); }
	ffssize matchf(const char *fmt, ...) const {
		va_list va;
		va_start(va, fmt);
		ffssize r = ffstr_matchfmtv(this, fmt, va);
		va_end(va);
		return r;
	}
};
