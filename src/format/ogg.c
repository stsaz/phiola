/** phiola: OGG input/output.
2015, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define verblog(t, ...)  phi_verblog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

#include <format/ogg-write.h>
#include <format/ogg-read.h>

#include <format/opus-meta.h>
#include <format/vorbis-meta.h>
