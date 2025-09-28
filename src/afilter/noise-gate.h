/** phiola: simple noise gate
2025, Simon Zolin */

#include <track.h>
#include <ffsys/std.h>
#include <ffbase/args.h>

static int arg_help()
{
	static const char help[] = "\n\
Noise Gate options:\n\
  threshold   Integer (dB) (=25)\n\
  release     Integer (msec) (=250)\n\
\n";
	ffstdout_write(help, FF_COUNT(help));
	return 1;
}

struct noise_gate_conf {
	unsigned threshold_db;
	unsigned release_msec;
};

#define O(m)  (void*)(ffsize)FF_OFF(struct noise_gate_conf, m)
static const struct ffarg noise_gate_conf_args[] = {
	{ "help",		'0',	arg_help },
	{ "release",	'u',	O(release_msec) },
	{ "threshold",	'u',	O(threshold_db) },
	{}
};
#undef O


struct noise_gate {
	struct noise_gate_conf conf;
	double threshold;
	uint64 release_samples, release_counter;
	unsigned channels;
	unsigned sample_size;
	unsigned state;
	unsigned opened;
};

static void* noise_gate_open(phi_track *t)
{
	struct noise_gate_conf cc = {
		.threshold_db = 25,
		.release_msec = 250,
	};
	struct ffargs a = {};
	if (ffargs_process_line(&a, noise_gate_conf_args, &cc, FFARGS_O_PARTIAL | FFARGS_O_DUPLICATES, t->conf.afilter.noise_gate)) {
		errlog(t, "%s", a.error);
		return PHI_OPEN_ERR;
	}

	struct noise_gate *c = phi_track_allocT(t, struct noise_gate);
	c->conf = cc;
	c->threshold = db_gain(-(int)c->conf.threshold_db);
	c->channels = t->audio.format.channels;
	c->sample_size = pcm_size(PHI_PCM_FLOAT64, c->channels);
	c->release_samples = pcm_samples(c->conf.release_msec, t->audio.format.rate);
	return c;
}

static void noise_gate_close(void *ctx, phi_track *t)
{
	struct noise_gate *c = ctx;
	phi_track_free(t, c);
}

static int request_input_conversion(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.conv"), PHI_TF_PREV))
		return PHI_ERR;

	t->aconv.in = t->audio.format;
	t->aconv.out = t->audio.format;
	t->aconv.out.format = PHI_PCM_FLOAT64;
	t->aconv.out.interleaved = 1;
	if (!t->conf.oaudio.format.format)
		t->conf.oaudio.format.format = t->audio.format.format;
	t->oaudio.format = t->aconv.out;
	t->data_out = t->data_in;
	return PHI_BACK;
}

/*
Level   Gate
===============
 30 dB  Closed
 20 dB  Opened
<25 dB  Opened for next 250 msec
*/
static int noise_gate_process(void *ctx, phi_track *t)
{
	struct noise_gate *c = ctx;

	switch (c->state) {
	case 0:
	case 1:
		if (!(t->oaudio.format.interleaved
			&& t->oaudio.format.format == PHI_PCM_FLOAT64)) {

			if (c->state == 0) {
				c->state = 1;
				return request_input_conversion(t);
			}

			errlog(t, "input audio format not supported");
			return PHI_ERR;
		}

		c->state = 2;
	}

	double *d = (double*)t->data_in.ptr;
	size_t samples = t->data_in.len / c->sample_size;
	for (size_t i = 0;  i < samples;  i++, d += c->channels) {

		double val = d[0];
		for (unsigned ic = 1;  ic < c->channels;  ic++) {
			val += d[ic];
		}

		if (!c->opened) {
			if (val < c->threshold) {
				for (unsigned ic = 0;  ic < c->channels;  ic++) {
					d[ic] = 0.0;
				}
				continue;
			}
			c->opened = 1;
			dbglog(t, "opened");
			continue;
		}

		if (val < c->threshold) {
			if (++c->release_counter >= c->release_samples) {
				c->release_counter = 0;
				c->opened = 0;
				dbglog(t, "closed");
			}
			continue;
		}

		c->release_counter = 0;
	}

	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

const phi_filter phi_noise_gate = {
	noise_gate_open, noise_gate_close, noise_gate_process,
	"noise-gate"
};
