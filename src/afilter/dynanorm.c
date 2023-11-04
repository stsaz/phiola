/** phiola: Dynamic Audio Normalizer filter.
2018, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <DynamicAudioNormalizer/DynamicAudioNormalizer-ff.h>
#include <ffsys/std.h>
#include <ffsys/globals.h>
#include <ffbase/args.h>

static const phi_core *core;
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)

static int arg_help()
{
	static const char help[] = "\n\
Dynamic Audio Normalizer options:\n\
  frame       Integer\n\
  size        Integer\n\
  peak        Float\n\
  max-amp     Float\n\
  target-rms  Float\n\
  compress    Float\n\
\n";
	ffstdout_write(help, FF_COUNT(help));
	return 1;
}

#define O(m)  (void*)(ffsize)FF_OFF(struct dynanorm_conf, m)
static const struct ffarg danorm_conf_args[] = {
	{ "compress",	'F',	O(compressFactor) },
	{ "frame",		'u',	O(frameLenMsec) },
	{ "help",		'0',	arg_help },
	{ "max-amp",	'F',	O(maxAmplification) },
	{ "peak",		'F',	O(peakValue) },
	{ "size",		'u',	O(filterSize) },
	{ "target-rms",	'F',	O(targetRms) },
	{}
};
#undef O

struct danorm {
	uint state;
	void *ctx;
	ffvec buf;
	uint off;
	struct phi_af fmt;
	struct dynanorm_conf conf;
};

static void* danorm_f_open(phi_track *t)
{
	struct danorm *c = ffmem_new(struct danorm);
	dynanorm_init(&c->conf);
	c->conf.channels = t->audio.format.channels;
	c->conf.sampleRate = t->audio.format.rate;
	struct ffargs a = {};
	if (!!ffargs_process_line(&a, danorm_conf_args, &c->conf, FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES, t->conf.afilter.danorm)) {
		errlog(t, "%s", a.error);
		ffmem_free(c);
		return PHI_OPEN_ERR;
	}
	return c;
}

static void danorm_f_close(void *ctx, phi_track *t)
{
	struct danorm *c = ctx;
	dynanorm_close(c->ctx);
	ffvec_free(&c->buf);
	ffmem_free(c);
}

/** Set array elements to point to consecutive regions of one buffer */
static inline void arrp_setbuf(void **ar, ffsize n, const void *buf, ffsize region_len)
{
	for (ffsize i = 0;  i != n;  i++) {
		ar[i] = (char*)buf + region_len * i;
	}
}

#define pcm_samples(time_ms, rate)  ((uint64)(time_ms) * (rate) / 1000)

static int danorm_f_process(void *ctx, phi_track *t)
{
	struct danorm *c = ctx;
	ffssize r;

	switch (c->state) {
	case 0:
		if (!(t->audio.format.format == PHI_PCM_FLOAT64
			&& !t->audio.format.interleaved)) {

			if (!core->track->filter(t, core->mod("afilter.conv"), PHI_TF_PREV))
				return PHI_ERR;

			t->aconv.in = t->audio.format;
			t->aconv.out = t->audio.format;
			t->aconv.out.format = PHI_PCM_FLOAT64;
			t->aconv.out.interleaved = 0;
			if (!t->conf.oaudio.format.format)
				t->conf.oaudio.format.format = t->audio.format.format;
			t->audio.format = t->aconv.out;
			t->data_out = t->data_in;
			c->state = 1;
			return PHI_BACK;
		}
		// fallthrough

	case 1: {
		if (0 != dynanorm_open(&c->ctx, &c->conf)) {
			errlog(t, "dynanorm_open()");
			return PHI_ERR;
		}

		uint ch = t->audio.format.channels;
		ffsize cap = pcm_samples(c->conf.frameLenMsec, t->audio.format.rate);
		ffvec_alloc(&c->buf, sizeof(void*) * ch + cap * sizeof(double) * ch, 1);
		if (t->audio.format.channels > 8)
			return PHI_ERR;
		c->buf.len = cap;
		arrp_setbuf((void**)c->buf.ptr, ch, c->buf.ptr + sizeof(void*) * ch, cap * sizeof(double));
		c->fmt = t->audio.format;
		c->state = 2;
		break;
	}
	}

	if (t->audio.seek_req) {
		dynanorm_reset(c->ctx);
		return PHI_MORE;
	}

	if (t->chain_flags & PHI_FFWD)
		c->off = 0;

	ffbool done = 0;
	uint sampsize = pcm_size1(&c->fmt);
	void *in[8];
	ffsize samples;
	void **datani = (void**)t->data_in.ptr;
	while (t->data_in.len != 0) {
		for (uint i = 0;  i != c->fmt.channels;  i++) {
			in[i] = (char*)datani[i] + c->off;
		}
		samples = t->data_in.len / sampsize;
		ffsize in_samps = samples;
		r = dynanorm_process(c->ctx, (const double*const*)in, &samples, (double**)c->buf.ptr, c->buf.len);
		dbglog(t, "output:%L  input:%L/%L", r, samples, in_samps);
		t->data_in.len -= samples * sampsize;
		c->off += samples * sampsize;
		if (r < 0) {
			errlog(t, "dynanorm_process()");
			return PHI_ERR;
		} else if (r != 0) {
			goto data;
		}
	}

	if (!(t->chain_flags & PHI_FFIRST))
		return PHI_MORE;

	r = dynanorm_process(c->ctx, NULL, NULL, (double**)c->buf.ptr, c->buf.len);
	if (r < 0) {
		errlog(t, "dynanorm_process()");
		return PHI_ERR;
	}
	dbglog(t, "output:%L", r);
	done = ((ffsize)r < c->buf.len);

data:
	ffstr_set(&t->data_out, c->buf.ptr, r * sampsize);
	return (done) ? PHI_DONE : PHI_DATA;
}

static const phi_filter phi_danorm = {
	danorm_f_open, danorm_f_close, danorm_f_process,
	"dyn-audio-norm"
};


static const void* danorm_iface(const char *name)
{
	if (ffsz_eq(name, "f")) return &phi_danorm;
	return NULL;
}

static const phi_mod phi_mod_danorm = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	danorm_iface
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	return &phi_mod_danorm;
}
