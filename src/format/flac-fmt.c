/** phiola: .flac reader
2018, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>

extern const phi_core *core;
#define syserrlog(t, ...)  phi_syserrlog(core, NULL, t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <format/flac-read.h>
#include <format/flac-write.h>
