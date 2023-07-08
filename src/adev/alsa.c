/** phiola: ALSA
2015, Simon Zolin */

#include <track.h>
#include <FFOS/ffos-extern.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "alsa", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "alsa", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "alsa", t, __VA_ARGS__)

#include <adev/audio.h>

struct alsa_mod {
	ffaudio_buf *out;
	uint buffer_length_msec;
	phi_timer tmr;
	struct phi_af fmt;
	ffvec fmts; // struct fmt_pair[]
	audio_out *usedby;
	uint dev_idx;
	uint init_ok :1;
};
static struct alsa_mod *mod;

static void alsa_buf_close(void *param)
{
	if (mod->out == NULL) return;

	dbglog(NULL, "free buffer");
	ffalsa.free(mod->out);
	mod->out = NULL;
}

static int alsa_init(phi_track *t)
{
	if (mod->init_ok)
		return 0;

	ffaudio_init_conf conf = {};
	if (0 != ffalsa.init(&conf)) {
		errlog(t, "init: %s", conf.error);
		return -1;
	}
	mod->init_ok = 1;
	return 0;
}

#include <adev/alsa-rec.h>
#include <adev/alsa-play.h>


static int alsa_adev_list(struct phi_adev_ent **ents, uint flags)
{
	int r;
	if (0 > (r = audio_dev_list(core, &ffalsa, ents, flags, "alsa")))
		return -1;
	return r;
}

static const phi_adev_if phi_alsa_dev = {
	.list = &alsa_adev_list,
	.list_free = audio_dev_listfree,
};


static const void* alsa_iface(const char *name)
{
	static const struct map_sz_vptr m[] = {
		{ "dev", &phi_alsa_dev },
		{ "play", &phi_alsa_play },
		{ "rec", &phi_alsa_rec },
		{}
	};
	return map_sz_vptr_find(m, name);
}

static void alsa_destroy(void)
{
	alsa_buf_close(NULL);
	ffvec_free(&mod->fmts);
	ffmem_free(mod);
	mod = NULL;
	ffalsa.uninit();
}

static const phi_mod phi_mod_alsa = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = alsa_iface,
	.close = alsa_destroy,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_new(struct alsa_mod);
	return &phi_mod_alsa;
}
