/** phiola: file I/O filters
2023, Simon Zolin */

#include <track.h>

extern phi_core *core;
#define syserrlog(t, ...)  phi_syserrlog(core, NULL, t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define infolog(t, ...)  phi_infolog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <core/file-read.h>
#include <core/file-write.h>
#include <core/file-stdin.h>
#include <core/file-stdout.h>
