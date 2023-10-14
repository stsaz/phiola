/** phiola: OSS
2017, Simon Zolin */

#include <track.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "oss", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "oss", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "oss", t, __VA_ARGS__)

#include <adev/audio-dev.h>
#include <adev/audio-play.h>
#include <adev/audio-rec.h>

typedef struct oss_mod {
	phi_timer tmr;
	ffaudio_buf *out;
	struct phi_af fmt;
	audio_out *usedby;
	const phi_track *track;
	uint dev_idx;
} oss_mod;

static oss_mod *mod;

#include <adev/oss-rec.h>
#include <adev/oss-play.h>

static void oss_destroy(void)
{
	if (mod == NULL)
		return;

	ffoss.free(mod->out);
	ffoss.uninit();
	ffmem_free(mod);
	mod = NULL;
}

static int mod_init(phi_track *t)
{
	return 0;
}

static int oss_adev_list(struct phi_adev_ent **ents, uint flags)
{
	if (0 != mod_init(NULL))
		return -1;

	int r;
	if (0 > (r = audio_dev_list(core, &ffoss, ents, flags, "oss")))
		return -1;
	return r;
}

static const phi_adev_if phi_oss_adev = {
	.list = &oss_adev_list,
	.listfree = &audio_dev_listfree,
};

static const void* oss_iface(const char *name)
{
	if (!ffsz_cmp(name, "out")) {
		return &phi_oss_play;
	} else if (!ffsz_cmp(name, "in")) {
		return &phi_oss_rec;
	} else if (!ffsz_cmp(name, "adev")) {
		return &phi_oss_adev;
	}
	return NULL;
}

static const phi_mod phi_oss_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	.iface = oss_iface,
	.destroy = oss_destroy,
	.conf = oss_conf,
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	mod = ffmem_new(oss_mod);
	return &phi_oss_mod;
}
