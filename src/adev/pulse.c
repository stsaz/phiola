/** phiola: PulseAudio
2017, Simon Zolin */

#include <track.h>
#include <FFOS/ffos-extern.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "pulse", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "pulse", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "pulse", t, __VA_ARGS__)

#include <adev/audio.h>

struct pulse_mod {
	phi_timer tmr;
	ffaudio_buf *out;
	uint buffer_length_msec;
	struct phi_af fmt;
	audio_out *usedby;
	uint dev_idx;
	uint init_ok :1;
};
static struct pulse_mod *mod;

void pulse_buf_close()
{
	if (mod->out == NULL) return;
	dbglog(NULL, "free");
	ffpulse.free(mod->out);
	mod->out = NULL;
}

static int pulse_init(phi_track *t)
{
	if (mod->init_ok) return 0;

	dbglog(t, "init");
	ffaudio_init_conf conf = {
		.app_name = "phiola",
	};
	if (0 != ffpulse.init(&conf)) {
		errlog(t, "init: %s", conf.error);
		return -1;
	}

	mod->init_ok = 1;
	return 0;
}


static int pulse_adev_list(struct phi_adev_ent **ents, uint flags)
{
	if (0 != pulse_init(NULL)) return -1;

	int r;
	if (0 > (r = audio_dev_list(core, &ffpulse, ents, flags, "pulse")))
		return -1;
	return r;
}

static const phi_adev_if phi_pulse_dev = {
	.list = pulse_adev_list,
	.list_free = audio_dev_listfree,
};


#include <adev/pulse-play.h>
#include <adev/pulse-rec.h>

static void pulse_mod_close()
{
	pulse_buf_close();
	dbglog(NULL, "uninit");
	ffpulse.uninit();
	ffmem_free(mod);  mod = NULL;
}

static const void* pulse_iface(const char *name)
{
	static const struct map_sz_vptr m[] = {
		{ "dev", &phi_pulse_dev },
		{ "play", &phi_pulse_play },
		{ "rec", &phi_pulse_rec },
		{}
	};
	return map_sz_vptr_find(m, name);
}

static const phi_mod phi_pulse_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = pulse_iface,
	.close = pulse_mod_close,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_new(struct pulse_mod);
	return &phi_pulse_mod;
}
