/** stdout logger
2022, Simon Zolin */

#pragma once
#include <ffsys/file.h>
#include <ffsys/std.h>
#include <ffsys/thread.h> // optional

typedef void (*zzlog_func)(ffstr s);

struct zzlog {
	zzlog_func func;
	fffd fd;

	char date[32];
	char levels[10][8];
	char colors[10][8];
	ffuint use_color :1;
	ffuint fd_file :1; // Windows: fd is a regular file, not console
};

#define ZZLOG_SYS_ERROR  0x10

/**
flags: level(0..9) + ZZLOG_SYS_ERROR

TIME #TID LEVEL CTX: [ID:] MSG [: (SYSCODE) SYSERR]
*/
static inline void zzlog_printv(struct zzlog *l, ffuint flags, const char *ctx, const char *id, const char *fmt, va_list va)
{
	ffuint level = flags & 0x0f;
	char buffer[4*1024];
	char *d = buffer;
	ffsize r = 0, cap = sizeof(buffer) - 10;

	const char *color_end = "";
	if (l->use_color) {
		const char *color = l->colors[level];
		if (color[0] != '\0') {
			r = _ffs_copyz(d, cap, color);
			color_end = FFSTD_CLR_RESET;
		}
	}

	r += _ffs_copyz(&d[r], cap - r, l->date);
	d[r++] = ' ';

#ifdef FFTHREAD_NULL
	ffuint64 tid = ffthread_curid();
	d[r++] = '#';
	r += ffs_fromint(tid, &d[r], cap - r, 0);
	d[r++] = ' ';
#endif

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

	if (l->func) {
		ffstr s = FFSTR_INITN(d, r);
		l->func(s);
		return;
	}

#ifdef FF_WIN
	if (!l->fd_file) {
		wchar_t ws[1024], *w;
		ffsize nw = FF_COUNT(ws);
		w = ffs_alloc_buf_utow(ws, &nw, (char*)d, r);

		DWORD written;
		WriteConsoleW(l->fd, w, nw, &written, NULL);

		if (w != ws)
			ffmem_free(w);
		return;
	}
#endif
	fffile_write(l->fd, d, r);
}

/** Add line to log */
static inline void zzlog_print(struct zzlog *l, ffuint flags, const char *ctx, const char *id, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	zzlog_printv(l, flags, ctx, id, fmt, va);
	va_end(va);
}
