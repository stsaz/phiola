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

	t->aconv.in = (t->oaudio.format.format) ? t->oaudio.format : t->audio.format;
	t->aconv.out = t->aconv.in;
	t->aconv.out.format = PHI_PCM_32;
	t->aconv.out.interleaved = 1;
	t->oaudio.format = t->aconv.out;
	t->data_out = t->data_in;
	return PHI_BACK;
}

struct sox_eq_conf {
	const char *type, *frequency, *width, *gain;
};

#define O(m)  (void*)(ffsize)FF_OFF(struct sox_eq_conf, m)
static const struct ffarg sox_eq_args[] = {
	{ "frequency",	's',	O(frequency) },
	{ "gain",		's',	O(gain) },
	{ "type",		's',	O(type) },
	{ "width",		's',	O(width) },
	{}
};
#undef O

enum {
	EQ_BAND,
	EQ_SHELVE_BASS,
	EQ_SHELVE_TREBLE,
	EQ_EMPTY,
};

static int sox_argv_extract(struct sox *c, phi_track *t, ffstr *s, struct sox_eq_conf *eqc)
{
	ffstr sc = {};
	ffstr_splitby(s, ',', &sc, s);
	ffstr_skipchar(s, ' ');
	sc.ptr[sc.len] = '\0';

	struct ffargs a = {};
	if (ffargs_process_line(&a, sox_eq_args, eqc, FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES, sc.ptr)) {
		errlog(t, "%s", a.error);
		return -1;
	}

	if (!a.argi)
		return EQ_EMPTY;

	if (!eqc->type)
		return EQ_BAND;

	static const struct {
		char name[7];
		u_char type;
	} eq_types[] = {
		{ "band",	EQ_BAND },
		{ "bass",	EQ_SHELVE_BASS },
		{ "treble",	EQ_SHELVE_TREBLE },
	};
	for (uint i = 0;  i < FF_COUNT(eq_types);  i++) {
		if (ffsz_eq(eqc->type, eq_types[i].name))
			return eq_types[i].type;
	}
	return -1;
}

static int sox_filter_add(struct sox *c, phi_track *t, ffstr *conf, uint index)
{
	int r;
	uint ia;
	const char *argv[3] = {}, *filter_name;
	struct sox_eq_conf eqc = {};

	switch ((r = sox_argv_extract(c, t, conf, &eqc))) {
	case EQ_BAND:
		if (!eqc.gain || !eqc.frequency || !eqc.width)
			goto err;
		argv[0] = eqc.frequency;
		argv[1] = eqc.width;
		argv[2] = eqc.gain;
		ia = 3;
		filter_name = "equalizer";
		break;

	case EQ_SHELVE_BASS:
	case EQ_SHELVE_TREBLE:
		if (!eqc.gain)
			goto err;
		argv[0] = eqc.gain;
		ia = 1;
		if (eqc.frequency)
			argv[ia++] = eqc.frequency;
		if (eqc.width)
			argv[ia++] = eqc.width;
		filter_name = eqc.type;
		break;

	case EQ_EMPTY:
		return 0;

	default:
		goto err;
	}

	dbglog(t, "adding filter #%u '%s': %s %s %s"
		, index, filter_name, argv[0], argv[1], argv[2]);
	if (phi_sox_filter(c->sox, filter_name, argv, ia)) {
		goto err;
	}

	return 0;

err:
	errlog(t, "Equalizer: incorrect parameters");
	t->error = PHI_E_FILTER_CONF;
	return PHI_ERR;
}

static void* sox_mem_alloc(void *opaque, uint n)
{
	return phi_track_alloc(opaque, n);
}

static void sox_mem_free(void *opaque, void *ptr)
{
	phi_track_free(opaque, ptr);
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

			.mem_alloc = sox_mem_alloc,
			.mem_free = sox_mem_free,
			.opaque = t,
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
			if (sox_filter_add(c, t, &s, i))
				return PHI_ERR;
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
