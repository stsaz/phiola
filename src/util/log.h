/** Logger
2022, Simon Zolin */

#pragma once
#include <ffsys/file.h>
#include <ffsys/std.h>

typedef void (*zzlog_func)(void *udata, ffstr s);

struct zzlog {
	void *udata;
	zzlog_func func;
	fffd fd;

	char levels[10][8];
	char colors[10][8];
	ffuint use_color :1;
	ffuint fd_file :1; // Windows: fd is a regular file, not console
};

#define ZZLOG_SYS_ERROR  0x10

/** Pass data to user or write to console or file. */
static void zzlog_write(struct zzlog *l, const char *d, unsigned n)
{
	if (l->func) {
		ffstr s = FFSTR_INITN(d, n);
		l->func(l->udata, s);
		return;
	}

#ifdef FF_WIN
	if (!l->fd_file) {
		wchar_t ws[1024], *w;
		ffsize nw = FF_COUNT(ws);
		w = ffs_alloc_buf_utow(ws, &nw, (char*)d, n);

		DWORD written;
		WriteConsoleW(l->fd, w, nw, &written, NULL);

		if (w != ws)
			ffmem_free(w);
		return;
	}
#endif

	fffile_write(l->fd, d, n);
}

/** Construct a log message.
flags: level(0..9) + ZZLOG_SYS_ERROR + ZZLOG_CAN_FLUSH
buffer: buffer with at least 10 bytes capacity

TIME [#TID] LEVEL [CTX:] [ID:] MSG [: (SYSCODE) SYSERR]
*/
static inline unsigned zzlog_build(struct zzlog *l, unsigned flags, char *buffer, size_t cap, const char *date, ffuint64 tid, const char *ctx, const char *id, const char *fmt, va_list va)
{
	ffuint level = flags & 0x0f;
	char *d = buffer;
	ffsize r = 0;
	cap -= 10;

	const char *color_end = "";
	if (l->use_color) {
		const char *color = l->colors[level];
		if (color[0] != '\0') {
			r = _ffs_copyz(d, cap, color);
			color_end = FFSTD_CLR_RESET;
		}
	}

	r += _ffs_copyz(&d[r], cap - r, date);
	d[r++] = ' ';

	if (tid != 0) {
		d[r++] = '#';
		r += ffs_from_uint_10(tid, &d[r], cap - r);
		d[r++] = ' ';
	}

	r += _ffs_copyz(&d[r], cap - r, l->levels[level]);
	d[r++] = ' ';

	if (ctx != NULL) {
		r += _ffs_copyz(&d[r], cap - r, ctx);
		d[r++] = ':';
		d[r++] = ' ';
	}

	if (id != NULL) {
		r += _ffs_copyz(&d[r], cap - r, id);
		d[r++] = ':';
		d[r++] = ' ';
	}

	ffssize r2 = ffs_formatv(&d[r], cap - r, fmt, va);
	if (r2 < 0)
		r2 = 0;
	r += r2;

	if (flags & ZZLOG_SYS_ERROR) {
		int e = fferr_last();
		r += ffs_format_r0(&d[r], cap - r, ": (%u) %s"
			, e, fferr_strptr(e));
	}

	r += _ffs_copyz(&d[r], cap - r, color_end);

#ifdef FF_WIN
	d[r++] = '\r';
#endif
	d[r++] = '\n';
	return r;
}

static inline int zzlog_printv(struct zzlog *l, unsigned flags, const char *date, ffuint64 tid, const char *ctx, const char *id, const char *fmt, va_list va)
{
	char buf[4096];
	va_list args;
	va_copy(args, va);
	unsigned n = zzlog_build(l, flags, buf, sizeof(buf), date, tid, ctx, id, fmt, va);
	va_end(args);

	zzlog_write(l, buf, n);
	return 0;
}

/** Add line to log */
static inline void zzlog_print(struct zzlog *l, unsigned flags, const char *date, ffuint64 tid, const char *ctx, const char *id, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	zzlog_printv(l, flags, date, tid, ctx, id, fmt, va);
	va_end(va);
}
