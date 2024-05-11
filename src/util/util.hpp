/** C++ utility functions
2023, Simon Zolin */

#include <ffbase/string.h>
#include <ffbase/vector.h>
#include <new>

struct xxstr : ffstr {
	xxstr() { ptr = NULL;  len = 0; }
	xxstr(ffstr s) { ptr = s.ptr;  len = s.len; }
	xxstr(const char *sz) { ptr = (char*)sz;  len = ffsz_len(sz); }
	void	operator=(const char *sz) { ptr = (char*)sz, len = ffsz_len(sz); }
	bool	equals(xxstr s) const { return ffstr_eq2(this, &s); }
	bool	equals_i(const char *sz) const { return ffstr_ieqz(this, sz); }
	char	at(size_t i) const { FF_ASSERT(i < len); return ptr[i]; }
	void	reset() { ptr = NULL;  len = 0; }
	void	free() { ffmem_free(ptr);  ptr = NULL;  len = 0; }
	xxstr&	shift(ffsize n) { ffstr_shift(this, n); return *this; }
	ffssize	split_by(char by, xxstr *left, xxstr *right) const { return ffstr_splitby(this, by, left, right); }
	ffssize	split_at(ffssize index, xxstr *left, xxstr *right) const { return ffstr_split(this, index, left, right); }
	ffssize	find_char(char c) const { return ffstr_findchar(this, c); }
	ffssize	find_str(xxstr s) const { return ffstr_findstr(this, &s); }
	ffssize	match_f(const char *fmt, ...) const {
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
	ushort uint16(ushort _default) const {
		ushort n;
		if (!ffstr_toint(this, &n, FFS_INT16))
			n = _default;
		return n;
	}
	uint uint32(uint _default) const {
		uint n;
		if (!ffstr_toint(this, &n, FFS_INT32))
			n = _default;
		return n;
	}
};

template<uint N> struct xxstr_buf : ffstr {
	char buf[N];
	xxstr_buf() { ptr = buf;  len = 0; }
	const char* zfmt(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		len = ffsz_formatv(ptr, N, fmt, va);
		va_end(va);
		return ptr;
	}
	ffstr& fmt(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		len = ffs_formatv(ptr, N, fmt, va);
		va_end(va);
		return *this;
	}
	xxstr_buf<N>& add_char(char ch) {
		ffstr_addchar(this, N - len, ch);
		return *this;
	}
};

template<uint N> struct xxwstr_buf {
	size_t len;
	wchar_t *ptr;
	wchar_t buf[N];
	xxwstr_buf() { ptr = buf;  len = 0; }
	const wchar_t* utow(const char *s) {
		len = N;
		return (ptr = ffs_utow(buf, &len, s, -1));
	}
};

struct xxvec : ffvec {
	xxvec() { ffvec_null(this); }
	xxvec(ffstr s) {
		ptr = s.ptr, len = s.len, cap = (s.len == 0 && s.ptr != NULL) ? 1 : s.len;
	}
	xxvec(ffslice s) {
		ptr = s.ptr, len = s.len, cap = (s.len == 0 && s.ptr != NULL) ? 1 : s.len;
	}
	~xxvec() { ffvec_free(this); }
	void free() { ffvec_free(this); }
	void reset() { ffvec_null(this); }
	xxvec& set(const char *sz) {
		ffvec_free(this);
		ptr = (void*)sz, len = ffsz_len(sz);
		return *this;
	}
	xxvec& acquire(ffstr s) {
		ffvec_free(this);
		ptr = s.ptr, len = s.len, cap = s.len;
		return *this;
	}
	xxvec& copy(ffstr s) {
		len = 0;
		ffvec_addstr(this, &s);
		return *this;
	}
	xxvec& add(xxstr s) {
		ffvec_addstr(this, &s);
		return *this;
	}
	xxvec& add_f(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		ffstr_growfmtv((ffstr*)this, &cap, fmt, va);
		va_end(va);
		return *this;
	}
	template<class T> T at(ffsize i) { return *ffslice_itemT(this, i, T); }
	template<class T> T* alloc(ffsize n) { return ffvec_allocT(this, n, T); }
	template<class T> T* push() { return ffvec_pushT(this, T); }
	template<class T> T* push_z() { return ffvec_zpushT(this, T); }
	const xxstr& str() const { return *(xxstr*)this; }
	const ffslice& slice() const { return *(ffslice*)this; }
	char* strz() {
		if (len && ((char*)ptr)[len-1] != '\0')
			ffvec_addchar(this, '\0');
		return (char*)ptr;
	}
};
