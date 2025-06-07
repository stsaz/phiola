/** phiola: AAC ADTS (.aac) reader and stream-writer
2017, Simon Zolin */

#include <track.h>

#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

extern const phi_core *core;

#include <format/aac-write.h>
