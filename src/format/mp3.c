/** phiola: MPEG Layer3 (.mp3) read/copy
2017, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <format/mp3-read.h>
#include <format/mp3-write.h>
