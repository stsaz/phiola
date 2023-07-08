/** phiola: Disable sleep timer on Linux via D-BUS.
2020, Simon Zolin */

#include <track.h>
#include <FFOS/process.h>
#include <util/linux-systimer.h>

static const phi_core *core;
#define dbglog(...)  phi_dbglog(core, "dbus-sleep", NULL, __VA_ARGS__)
#define errlog(...)  phi_errlog(core, "dbus-sleep", NULL, __VA_ARGS__)

#define SWITCH_TIMEOUT  5000

struct dbus_sleep {
	phi_timer tmr;
	uint paused;
	uint ntracks;
	struct ffps_systimer st;
};

static struct dbus_sleep *g;

static void switch_sleep_timer(void *param)
{
	uint n = FFINT_READONCE(g->ntracks);
	if (!g->paused && n != 0) {
		if (0 != ffps_systimer(&g->st, 1)) {
			errlog("ffps_systimer()");
			return;
		}
		g->paused = 1;
		dbglog("paused system sleep timer");

	} else if (g->paused && n == 0) {
		ffps_systimer(&g->st, 0);
		g->paused = 0;
		dbglog("resumed system sleep timer");
	}
}


static void* dbus_sleep_open(phi_track *d)
{
	if (ffint_fetch_add(&g->ntracks, 1) == 0) {
		core->timer(&g->tmr, -SWITCH_TIMEOUT, switch_sleep_timer, NULL);
	}
	return (void*)1;
}

static void dbus_sleep_close(void *ctx, phi_track *t)
{
	if (ffint_fetch_add(&g->ntracks, -1) == 1) {
		core->timer(&g->tmr, -SWITCH_TIMEOUT, switch_sleep_timer, NULL);
	}
}

static int dbus_sleep_process(void *ctx, phi_track *d)
{
	return PHI_DONE;
}

static const phi_filter phi_dbus_sleep = {
	dbus_sleep_open, dbus_sleep_close, dbus_sleep_process,
	"dbus-sleep"
};


static void dbus_sleep_destroy()
{
	if (g->paused)
		ffps_systimer(&g->st, 0);
	ffps_systimer_close(&g->st);
	ffmem_free(g); g = NULL;
}

static const void* dbus_sleep_iface(const char *name)
{
	if (ffsz_eq(name, "sleep")) return &phi_dbus_sleep;
	return NULL;
}

static const phi_mod phi_dbus_sleep_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	dbus_sleep_iface, dbus_sleep_destroy
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	g = ffmem_new(struct dbus_sleep);
	g->st.appname = "phiola";
	g->st.reason = "audio playback";
	return &phi_dbus_sleep_mod;
}
