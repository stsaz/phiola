/** phiola: C++ utility functions
2023, Simon Zolin */

#include <ffbase/string.h>
#include <ffbase/vector.h>

struct ffstrxx : ffstr {
	ffstrxx() { ptr = NULL;  len = 0; }
	ffstrxx(ffstr s) { ptr = s.ptr;  len = s.len; }
	ffstrxx(const char *sz) { ptr = (char*)sz;  len = ffsz_len(sz); }
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

struct ffvecxx : ffvec {
	ffvecxx() { ffvec_null(this); }
	ffvecxx(ffstr s) {
		ptr = s.ptr, len = s.len, cap = s.len;
	}
	~ffvecxx() { ffvec_free(this); }
	void free() { ffvec_free(this); }
	ffvecxx& set(const char *sz) {
		ffvec_free(this);
		ptr = (void*)sz, len = ffsz_len(sz);
		return *this;
	}
	ffvecxx& acquire(ffstr s) {
		ffvec_free(this);
		ptr = s.ptr, len = s.len, cap = s.len;
		return *this;
	}
	ffvecxx& copy(ffstr s) {
		len = 0;
		ffvec_addstr(this, &s);
		return *this;
	}
	const ffstr& str() const { return *(ffstr*)(this); }
};
