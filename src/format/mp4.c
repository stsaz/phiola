/** phiola: .mp4 read/write
2016, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>
#include <ffsys/globals.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define verblog(t, ...)  phi_verblog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <format/mp4-write.h>
