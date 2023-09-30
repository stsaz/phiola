/** phiola: C++ utility functions
2023, Simon Zolin */

#include <ffbase/string.h>
#include <ffbase/vector.h>

struct ffstrxx : ffstr {
	ffstrxx() { ptr = NULL;  len = 0; }
	ffstrxx(ffstr s) { ptr = s.ptr;  len = s.len; }
	ffstrxx(const char *sz) { ptr = (char*)sz;  len = ffsz_len(sz); }
	void operator=(const char *sz) { ptr = (char*)sz, len = ffsz_len(sz); }
	bool operator==(const char *sz) const { return ffstr_eqz(this, sz); }
	void reset() { ptr = NULL;  len = 0; }
	void free() { ffmem_free(ptr);  ptr = NULL;  len = 0; }
	ffstrxx shift(ffsize n) { ffstr_shift(this, n); return *this; }
	ffssize split(char by, ffstrxx *left, ffstrxx *right) const { return ffstr_splitby(this, by, left, right); }
	ffssize matchf(const char *fmt, ...) const {
		va_list va;
		va_start(va, fmt);
		ffssize r = ffstr_matchfmtv(this, fmt, va);
		va_end(va);
		return r;
	}
	short int16(short _default) const {
		short n;
		if (!ffstr_toint(this, &n, FFS_INT16 | FFS_INTSIGN))
			n = _default;
		return n;
	}
	u_short uint16(u_short _default) const {
		u_short n;
		if (!ffstr_toint(this, &n, FFS_INT16))
			n = _default;
		return n;
	}
	u_int uint32(u_int _default) const {
		u_int n;
		if (!ffstr_toint(this, &n, FFS_INT32))
			n = _default;
		return n;
	}
};

template<int N> struct ffstrxx_buf : ffstr {
	char buf[N];
	ffstrxx_buf() { ptr = buf;  len = 0; }
	const char* zfmt(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		len = ffsz_formatv(ptr, N, fmt, va);
		va_end(va);
		return ptr;
	}
};

struct ffvecxx : ffvec {
	ffvecxx() { ffvec_null(this); }
	ffvecxx(ffstr s) {
		ptr = s.ptr, len = s.len, cap = s.len;
	}
	~ffvecxx() { ffvec_free(this); }
	void free() { ffvec_free(this); }
	void reset() { ffvec_null(this); }
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
	template<class T> T* alloc(ffsize n) { return ffvec_allocT(this, n, T); }
	template<class T> T* push() { return ffvec_pushT(this, T); }
	const ffstrxx& str() const { return *(ffstrxx*)this; }
	const ffslice& slice() const { return *(ffslice*)this; }
};
