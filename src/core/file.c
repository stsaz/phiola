/** phiola: file I/O filters
2023, Simon Zolin */

#include <track.h>

extern const phi_core *core;
extern const phi_meta_if *phi_metaif;
#define syserrlog(t, ...)  phi_syserrlog(core, NULL, t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

static inline int frw_benchmark(fftime *t)
{
	if (core->conf.log_level >= PHI_LOG_DEBUG) {
		*t = core->time(NULL, PHI_CORE_TIME_MONOTONIC);
		return 1;
	}
	return 0;
}

#include <core/file-read.h>
#include <core/file-write.h>
#include <core/file-stdin.h>
#include <core/file-stdout.h>
#include <core/file-tee.h>
