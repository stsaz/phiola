/** phiola: afilter: sox filters
2026, Simon Zolin */

#include <track.h>
#include <sox/sox-phi.h>
#include <util/aformat.h>
#include <ffsys/globals.h>
#include <ffbase/args.h>

static const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct sox {
	uint state;
	sox_ctx *sox;
	ffstr in;
	char *args;
};

static void* sox_open(phi_track *t)
{
	struct sox *c = phi_track_allocT(t, struct sox);
	return c;
}

static void sox_close(struct sox *c, phi_track *t)
{
	phi_sox_destroy(c->sox);
	ffmem_free(c->args);
	phi_track_free(t, c);
}

static int request_input_conversion(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.conv"), PHI_TF_PREV))
		return PHI_ERR;

	t->aconv.in = t->audio.format;
	t->aconv.out = t->audio.format;
	t->aconv.out.format = PHI_PCM_32;
	t->aconv.out.interleaved = 1;
	t->oaudio.format = t->aconv.out;
	t->data_out = t->data_in;
	return PHI_BACK;
}

struct sox_eq_conf {
	const char *frequency, *width, *gain;
};

#define O(m)  (void*)(ffsize)FF_OFF(struct sox_eq_conf, m)
static const struct ffarg sox_eq_args[] = {
	{ "frequency",	's',	O(frequency) },
	{ "gain",		's',	O(gain) },
	{ "width",		's',	O(width) },
	{}
};
#undef O

static int sox_argv_extract(struct sox *c, phi_track *t, ffstr *s, char **argv, uint n)
{
	struct sox_eq_conf eqc = {};
	ffstr sc = {};
	ffstr_splitby(s, ',', &sc, s);
	ffstr_skipchar(s, ' ');
	sc.ptr[sc.len] = '\0';

	struct ffargs a = {};
	if (ffargs_process_line(&a, sox_eq_args, &eqc, FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES, sc.ptr)) {
		errlog(t, "%s", a.error);
		return -1;
	}

	if (!eqc.frequency || !eqc.width || !eqc.gain)
		return -1;

	argv[0] = (char*)eqc.frequency;
	argv[1] = (char*)eqc.width;
	argv[2] = (char*)eqc.gain;
	return 0;
}

static int sox_process(struct sox *c, phi_track *t)
{
	switch (c->state) {
	case 0:
	case 1: {
		if (!(t->oaudio.format.format == PHI_PCM_32
			&& t->oaudio.format.interleaved)) {

			if (c->state == 0) {
				c->state = 1;
				return request_input_conversion(t);
			}

			errlog(t, "Input format is not supported");
			return PHI_ERR;
		}

		struct sox_conf conf = {
			.rate = t->oaudio.format.rate,
			.channels = t->oaudio.format.channels,
		};
		if (phi_sox_create(&c->sox, &conf)) {
			errlog(t, "phi_sox_create");
			return PHI_ERR;
		}

		c->args = ffsz_dup(t->conf.afilter.equalizer);
		ffstr s = FFSTR_INITZ(c->args);
		dbglog(t, "conf: '%S'", &s);
		ffstr_skipchar(&s, ' ');

		uint i = 1;
		while (s.len) {
			char *argv[3];
			if (sox_argv_extract(c, t, &s, argv, 3)) {
				errlog(t, "Equalizer: incorrect parameters");
				t->error = PHI_E_FILTER_CONF;
				return PHI_ERR;
			}
			if (phi_sox_filter(c->sox, "equalizer", (const char**)argv, 3)) {
				errlog(t, "phi_sox_filter");
				return PHI_ERR;
			}
			dbglog(t, "added filter #%u 'equalizer': %s %s %s", i, argv[0], argv[1], argv[2]);
			i++;
		}

		phi_sox_filter(c->sox, NULL, NULL, 0);
		c->state = 2;
		break;
	}
	}

	if (t->chain_flags & PHI_FFWD)
		c->in = t->data_in;

	if (c->in.len == 0)
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_MORE;

	size_t n = c->in.len;
	t->data_out.len = phi_sox_process(c->sox, (int*)c->in.ptr, &n, (int**)&t->data_out.ptr);
	ffstr_shift(&c->in, n);
	return PHI_DATA;
}

static const phi_filter sox = {
	sox_open, (void*)sox_close, (void*)sox_process,
	"sox"
};

static const void* sox_mod_iface(const char *name)
{
	if (ffsz_eq(name, "sox")) return &sox;
	return NULL;
}

static const phi_mod sox_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	sox_mod_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &sox_mod;
}
