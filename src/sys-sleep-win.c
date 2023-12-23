/** phiola: Disable sleep timer on Windows.
2020, Simon Zolin */

#include <track.h>

extern const phi_core *core;
#define errlog(...)  phi_errlog(core, "win-sleep", NULL, __VA_ARGS__)
#define dbglog(...)  phi_dbglog(core, "win-sleep", NULL, __VA_ARGS__)

#include <sys-sleep.h>

const phi_filter phi_winsleep = {
	sys_sleep_open, sys_sleep_close, sys_sleep_process,
	"win-sleep"
};


void win_sleep_init()
{
	g = ffmem_new(struct sys_sleep);
}

void win_sleep_destroy()
{
	if (g == NULL) return;

	if (g->paused)
		ffps_systimer(&g->st, FFPS_SYSTIMER_DEFAULT);
	ffps_systimer_close(&g->st);
	ffmem_free(g);  g = NULL;
}
