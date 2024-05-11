/** phiola: executor: logs
2023, Simon Zolin */

static __thread uint64 thread_id;

static void exe_logv(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, va_list va)
{
	const char *id = (t) ? t->id : NULL;
	const char *ctx = (module || !t) ? module : (char*)x->core->track->cmd(t, PHI_TRACK_CUR_FILTER_NAME);

	if (x->core) {
		ffdatetime dt;
		fftime tm = x->core->time(&dt, 0);
		if (fftime_cmp(&tm, &x->time_last)) {
			x->time_last = tm;
			fftime_tostr1(&dt, x->log_date, sizeof(x->log_date), FFTIME_HMS_MSEC);
		}
	}

	uint64 tid = thread_id;
	if (tid == 0) {
		tid = ffthread_curid();
		thread_id = tid;
	}

	zzlog_printv(log_obj, flags, x->log_date, tid, ctx, id, fmt, va);
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
#define warnlog(...)  exe_log(&x->log, PHI_LOG_WARN, NULL, NULL, __VA_ARGS__)
#define infolog(...)  exe_log(&x->log, PHI_LOG_INFO, NULL, NULL, __VA_ARGS__)
#define dbglog(...) \
do { \
	if (x->debug) \
		exe_log(&x->log, PHI_LOG_DEBUG, NULL, NULL, __VA_ARGS__); \
} while (0)

static void logs(struct zzlog *l)
{
	if (!l->fd) {
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

		static const char colors[][8] = {
			/*PHI_LOG_ERR*/		FFSTD_CLR(FFSTD_RED),
			/*PHI_LOG_WARN*/	FFSTD_CLR(FFSTD_YELLOW),
			/*PHI_LOG_USER*/	"",
			/*PHI_LOG_INFO*/	FFSTD_CLR(FFSTD_GREEN),
			/*PHI_LOG_VERBOSE*/	FFSTD_CLR(FFSTD_GREEN),
			/*PHI_LOG_DEBUG*/	"",
			/*PHI_LOG_EXTRA*/	FFSTD_CLR_I(FFSTD_BLUE),
		};
		ffmem_copy(l->colors, colors, sizeof(colors));
	}

	l->fd = (!x->stdout_busy) ? ffstdout : ffstderr;
	int r = ffstd_attr(l->fd, FFSTD_VTERM, FFSTD_VTERM);
	l->use_color = !r;
	l->fd_file = (r < 0);
}
