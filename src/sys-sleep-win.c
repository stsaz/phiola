/** phiola: Disable sleep timer on Windows.
2020, Simon Zolin */

/** Pause/resume system sleep timer.
Pause system sleep timer when the first track is created.
Arm the timer upon completion of the last track.
If a new track is created before the timer expires - deactivate the timer.
Otherwise, resume system sleep timer. */

#include <track.h>

extern const phi_core *core;
#define dbglog(...)  phi_dbglog(core, "win-sleep", NULL, __VA_ARGS__)


/** Reset or disable a system timer.
flags:
 ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED: don't put the system to sleep
 ES_DISPLAY_REQUIRED: don't switch off display
 ES_CONTINUOUS:
  0: reset once
  1 + flags: disable
  1 + no flags: restore default behaviour
*/
#define ffps_systimer(flags)  SetThreadExecutionState(flags)


#define SWITCH_TIMEOUT  5000

struct winsleep {
	phi_timer tmr;
	uint paused;
	uint ntracks;
};

static struct winsleep *g;

static void switch_sleep_timer(void *param)
{
	uint n = FFINT_READONCE(g->ntracks);
	if (!g->paused && n != 0) {
		ffps_systimer(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
		g->paused = 1;
		dbglog("paused system sleep timer");

	} else if (g->paused && n == 0) {
		ffps_systimer(ES_CONTINUOUS);
		g->paused = 0;
		dbglog("resumed system sleep timer");
	}
}

static void* winsleep_open(phi_track *d)
{
	if (ffint_fetch_add(&g->ntracks, 1) == 0) {
		core->timer(0, &g->tmr, -SWITCH_TIMEOUT, switch_sleep_timer, NULL);
	}
	return (void*)1;
}

static void winsleep_close(void *ctx, phi_track *t)
{
	if (ffint_fetch_add(&g->ntracks, -1) == 1) {
		core->timer(0, &g->tmr, -SWITCH_TIMEOUT, switch_sleep_timer, NULL);
	}
}

static int winsleep_process(void *ctx, phi_track *d)
{
	return PHI_DONE;
}

const phi_filter phi_winsleep = {
	winsleep_open, winsleep_close, winsleep_process,
	"win-sleep"
};


void win_sleep_init()
{
	g = ffmem_new(struct winsleep);
}

void win_sleep_destroy()
{
	if (g == NULL) return;

	switch_sleep_timer(NULL);
	ffmem_free(g);
}
