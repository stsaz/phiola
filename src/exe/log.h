/** phiola: executor: logs
2023, Simon Zolin */

/** Check if fd is a terminal */
static int std_console(fffd fd)
{
#ifdef FF_WIN
	DWORD r;
	return GetConsoleMode(fd, &r);

#else
	fffileinfo fi;
	return (0 == fffile_info(fd, &fi)
		&& FFFILE_UNIX_CHAR == (fffileinfo_attr(&fi) & FFFILE_UNIX_TYPEMASK));
#endif
}

static void exe_logv(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, va_list va)
{
	const char *id = (t) ? t->id : NULL;
	const char *ctx = (module || !t) ? module : (char*)x->core->track->cmd(t, PHI_TRACK_CUR_FILTER_NAME);

	if (x->core) {
		ffdatetime dt;
		fftime tm = x->core->time(&dt, 0);
		if (fftime_cmp(&tm, &x->time_last)) {
			x->time_last = tm;
			fftime_tostr1(&dt, x->log.date, sizeof(x->log.date), FFTIME_HMS_MSEC);
		}
	}

	zzlog_printv(log_obj, flags, ctx, id, fmt, va);
}

static void exe_log(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	exe_logv(log_obj, flags, module, t, fmt, va);
	va_end(va);
}

#define syserrlog(...)  exe_log(&x->log, PHI_LOG_ERR | PHI_LOG_SYS, NULL, NULL, __VA_ARGS__)
#define errlog(...)  exe_log(&x->log, PHI_LOG_ERR, NULL, NULL, __VA_ARGS__)
#define infolog(...)  exe_log(&x->log, PHI_LOG_INFO, NULL, NULL, __VA_ARGS__)
#define dbglog(...) \
do { \
	if (x->debug) \
		exe_log(&x->log, PHI_LOG_DEBUG, NULL, NULL, __VA_ARGS__); \
} while (0)

static void logs(struct zzlog *l)
{
	static const char levels[][8] = {
		"ERROR ",
		"WARN  ",
		"INFO  ",
		"INFO  ",
		"INFO  ",
		"DEBUG ",
		"DEBUG+",
	};
	ffmem_copy(l->levels, levels, sizeof(levels));

	if (!x->stdout_busy) {
		l->fd = ffstdout;
#ifdef FF_WIN
		l->use_color = !ffstd_attr(ffstdout, FFSTD_VTERM, FFSTD_VTERM);
		l->fd_file = (!l->use_color && !std_console(ffstdout));
#else
		l->use_color = std_console(ffstdout);
#endif

	} else {

		l->fd = ffstderr;
#ifdef FF_WIN
		l->use_color = !ffstd_attr(ffstderr, FFSTD_VTERM, FFSTD_VTERM);
		l->fd_file = (!l->use_color && !std_console(ffstderr));
#else
		l->use_color = std_console(ffstderr);
#endif
	}

	static const char colors[][8] = {
		/*PHI_LOG_ERR*/	FFSTD_CLR(FFSTD_RED),
		/*PHI_LOG_WARN*/	FFSTD_CLR(FFSTD_YELLOW),
		/*PHI_LOG_USER*/	"",
		/*PHI_LOG_INFO*/	FFSTD_CLR(FFSTD_GREEN),
		/*PHI_LOG_VERBOSE*/	FFSTD_CLR(FFSTD_GREEN),
		/*PHI_LOG_DEBUG*/	"",
		/*PHI_LOG_EXTRA*/	FFSTD_CLR_I(FFSTD_BLUE),
	};
	ffmem_copy(l->colors, colors, sizeof(colors));
}
