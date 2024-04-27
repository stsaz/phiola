/** phiola: system sleep timer
2020, Simon Zolin */

/** Pause/resume system sleep timer.
. Pause system sleep timer when the first track is created.
. Arm the timer upon completion of the last track.
* If a new track is created before the timer expires - deactivate the timer.
* Otherwise, resume system sleep timer. */

#include <util/systimer.h>

#define SWITCH_TIMEOUT  5000

struct sys_sleep {
	phi_task task;
	phi_timer tmr;
	uint paused;
	uint ntracks;
	struct ffps_systimer st;
};

static struct sys_sleep *g;

static void switch_sleep_timer(void *param)
{
	uint n = FFINT_READONCE(g->ntracks);
	if (!g->paused && n != 0) {
		if (ffps_systimer(&g->st, FFPS_SYSTIMER_NOSLEEP)) {
			errlog("ffps_systimer()");
			return;
		}
		g->paused = 1;
		dbglog("paused system sleep timer");

	} else if (g->paused && n == 0) {
		ffps_systimer(&g->st, FFPS_SYSTIMER_DEFAULT);
		g->paused = 0;
		dbglog("resumed system sleep timer");
	}
}


static void sys_sleep_timer_arm(void *param)
{
	core->timer(0, &g->tmr, -SWITCH_TIMEOUT, switch_sleep_timer, NULL);
}

static void* sys_sleep_open(phi_track *d)
{
	if (ffint_fetch_add(&g->ntracks, 1) == 0) {
		core->task(0, &g->task, sys_sleep_timer_arm, NULL);
	}
	return (void*)1;
}

static void sys_sleep_close(void *ctx, phi_track *t)
{
	if (ffint_fetch_add(&g->ntracks, -1) == 1) {
		core->task(0, &g->task, sys_sleep_timer_arm, NULL);
	}
}

static int sys_sleep_process(void *ctx, phi_track *d) { return PHI_DONE; }
