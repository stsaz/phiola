/** phiola: audio device interface
2020, Simon Zolin */

#include <adev/audio.h>

static int audio_dev_list(const phi_core *core, const ffaudio_interface *audio, struct phi_adev_ent **ents, uint flags, const char *mod_name)
{
	ffvec a = {};
	ffaudio_dev *t;
	struct phi_adev_ent *e;
	int r, rr = -1;

	uint f;
	if (flags == PHI_ADEV_PLAYBACK)
		f = FFAUDIO_DEV_PLAYBACK;
	else if (flags == PHI_ADEV_CAPTURE)
		f = FFAUDIO_DEV_CAPTURE;
	else
		return -1;
	t = audio->dev_alloc(f);

	for (;;) {
		r = audio->dev_next(t);
		if (r == 1)
			break;
		else if (r < 0) {
			phi_errlog(core, mod_name, NULL, "dev_next(): %s", audio->dev_error(t));
			goto end;
		}

		e = ffvec_zpushT(&a, struct phi_adev_ent);

		if (NULL == (e->name = ffsz_dup(audio->dev_info(t, FFAUDIO_DEV_NAME))))
			goto end;

		e->default_device = !!audio->dev_info(t, FFAUDIO_DEV_IS_DEFAULT);

		const ffuint *def_fmt = (void*)audio->dev_info(t, FFAUDIO_DEV_MIX_FORMAT);
		if (def_fmt != NULL) {
			e->default_format.format = ffaudio_to_phi_af(def_fmt[0]);
			e->default_format.rate = def_fmt[1];
			e->default_format.channels = def_fmt[2];
		}
	}

	e = ffvec_zpushT(&a, struct phi_adev_ent);
	e->name = NULL;
	*ents = (void*)a.ptr;
	rr = a.len - 1;

end:
	audio->dev_free(t);
	if (rr < 0) {
		FFSLICE_WALK(&a, e) {
			ffmem_free(e->name);
		}
		ffvec_free(&a);
	}
	return rr;
}

static void audio_dev_listfree(struct phi_adev_ent *ents)
{
	struct phi_adev_ent *e;
	for (e = ents;  e->name != NULL;  e++) {
		ffmem_free(e->name);
	}
	ffmem_free(ents);
}
