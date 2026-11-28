/** phiola: playlist functions
2023, Simon Zolin */

#include <track.h>

extern const struct phi_core *core;
#define syserrlog(...)  phi_syserrlog(core, NULL, NULL, __VA_ARGS__)
#define syswarnlog(...)  phi_syswarnlog(core, NULL, NULL, __VA_ARGS__)
#define warnlog(...)  phi_warnlog(core, NULL, NULL, __VA_ARGS__)
#define infolog(...)  phi_infolog(core, NULL, NULL, __VA_ARGS__)
#define dbglog(...)  phi_dbglog(core, NULL, NULL, __VA_ARGS__)

#include <format/playlist-heal.h>

const phi_playlist_if pl_if = {
	pl_heal,
};
