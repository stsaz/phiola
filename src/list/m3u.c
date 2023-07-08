/** phiola: .m3u read/write */

#include <track.h>

extern const phi_core *core;
static const phi_queue_if *queue;
static const phi_meta_if *metaif;
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)

#include <list/entry.h>
#include <list/m3u-read.h>
#include <list/m3u-write.h>
