/** phiola: Disable sleep timer on Linux via D-BUS.
2020, Simon Zolin */

#include <track.h>
#include <ffsys/process.h>

static const phi_core *core;
#define dbglog(...)  phi_dbglog(core, "dbus-sleep", NULL, __VA_ARGS__)
#define errlog(...)  phi_errlog(core, "dbus-sleep", NULL, __VA_ARGS__)

#include <sys-sleep.h>

static const phi_filter phi_dbus_sleep = {
	sys_sleep_open, sys_sleep_close, sys_sleep_process,
	"dbus-sleep"
};


static void dbus_sleep_destroy()
{
	if (g->paused)
		ffps_systimer(&g->st, FFPS_SYSTIMER_DEFAULT);
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
	g = ffmem_new(struct sys_sleep);
	g->st.appname = "phiola";
	g->st.reason = "audio playback";
	return &phi_dbus_sleep_mod;
}
