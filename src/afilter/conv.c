/** phiola: audio converter
2019, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <util/aformat.h>
#include <ffaudio/pcm-convert.h>

#define errlog(trk, ...)  phi_errlog(core, NULL, trk, __VA_ARGS__)
#define dbglog(trk, ...)  phi_dbglog(core, NULL, trk, __VA_ARGS__)

extern const phi_core *core;

enum {
	CONV_OUTBUF_MSEC = 500,
};

struct aconv {
	uint state;
	uint out_samp_size;
	struct pcm_af fi, fo;
	ffstr in;
	ffvec buf;
	uint off;
};

static void* aconv_open(phi_track *t)
{
	struct aconv *c = phi_track_allocT(t, struct aconv);
	return c;
}

static void aconv_close(void *ctx, phi_track *t)
{
	struct aconv *c = ctx;
	phi_track_free(t, c->buf.ptr);
	phi_track_free(t, c);
}

static void log_pcmconv(int r, const struct phi_af *in, const struct phi_af *out, phi_track *t)
{
	int level = PHI_LOG_DEBUG;
	const char *unsupp = "";
	if (r != 0) {
		level = PHI_LOG_ERR;
		unsupp = " not supported";
	} else if (core->conf.log_level < PHI_LOG_DEBUG) {
		return;
	}

	char bufi[100], bufo[100];
	core->conf.log(core->conf.log_obj, level, NULL, t, "audio conversion%s: %s -> %s"
		, unsupp, phi_af_print(in, bufi, 100), phi_af_print(out, bufo, 100));
}

/** Set array elements to point to consecutive regions of one buffer */
static inline void arrp_setbuf(void **ar, ffsize n, const void *buf, ffsize region_len)
{
	for (ffsize i = 0;  i != n;  i++) {
		ar[i] = (char*)buf + region_len * i;
	}
}

static int aconv_prepare(struct aconv *c, phi_track *t)
{
	c->fi = *(struct pcm_af*)&t->aconv.in;
	c->fo = *(struct pcm_af*)&t->aconv.out;

	if (c->fi.rate != c->fo.rate) {
		if (!core->track->filter(t, core->mod("af-soxr.conv"), 0))
			return PHI_ERR;

		if (c->fi.channels == c->fo.channels
			&& c->fi.format != PHI_PCM_24) {
			// Next module is converting format and sample rate
			t->data_out = t->data_in;
			return PHI_DONE;
		}

		// We convert channels, the next module converts format and sample rate, e.g.:
		// [... --(int24/44.1/1)-> conv --(float/44.1/2)-> soxr --(int16/48/2)-> ... ]

		// our output format:
		c->fo.format = PHI_PCM_FLOAT32;
		c->fo.rate = c->fi.rate;

		// next module's input format:
		t->aconv.in.format = c->fo.format;
		t->aconv.in.channels = c->fo.channels;
		t->aconv.in.interleaved = c->fo.interleaved;
	}

	int r = pcm_convert(&c->fo, NULL, &c->fi, NULL, 0);
	log_pcmconv(r, (void*)&c->fi, (void*)&c->fo, t);
	if (r != 0)
		return PHI_ERR;

	// Allow no more than 16MB per 1 second of 64-bit 7.1 audio: 0x00ffffff/(64/8*8)=262143
	if (c->fo.rate > 262143) {
		errlog(t, "too large sample rate: %u", c->fo.rate);
		return PHI_ERR;
	}

	uint out_ch = c->fo.channels & PCM_CHAN_MASK;
	c->out_samp_size = pcm_size(c->fo.format, out_ch);
	size_t cap = msec_to_samples(CONV_OUTBUF_MSEC, c->fo.rate) * c->out_samp_size;
	uint n = cap;
	if (!c->fo.interleaved)
		n = sizeof(void*) * out_ch + cap;
	c->buf.ptr = phi_track_alloc(t, n);
	c->buf.cap = n;
	if (!c->fo.interleaved) {
		arrp_setbuf((void**)c->buf.ptr, out_ch, c->buf.ptr + sizeof(void*) * out_ch, cap / out_ch);
	}
	c->buf.len = cap / c->out_samp_size;

	return PHI_DATA;
}

static int aconv_process(void *ctx, phi_track *t)
{
	struct aconv *c = ctx;
	uint samples;
	int r;

	switch (c->state) {
	case 0:
		r = aconv_prepare(c, t);
		if (r != PHI_DATA)
			return r;
		c->state = 2;
		break;
	}

	if (t->chain_flags & PHI_FFWD) {
		c->in = t->data_in;
		t->data_in.len = 0;
		c->off = 0;
	}

	samples = (uint)ffmin(c->in.len / pcm_size1(&c->fi), c->buf.len);
	if (samples == 0) {
		if (t->chain_flags & PHI_FFIRST)
			return PHI_DONE;
		return PHI_MORE;
	}

	void *in[8];
	const void *data;
	if (!c->fi.interleaved) {
		void **datani = (void**)c->in.ptr;
		for (uint i = 0;  i != c->fi.channels;  i++) {
			in[i] = (char*)datani[i] + c->off;
		}
		data = in;
	} else {
		data = (char*)c->in.ptr + c->off * c->fi.channels;
	}

	if (0 != pcm_convert(&c->fo, c->buf.ptr, &c->fi, data, samples)) {
		return PHI_ERR;
	}

	ffstr_set(&t->data_out, c->buf.ptr, samples * c->out_samp_size);
	c->in.len -= samples * pcm_size1(&c->fi);
	c->off += samples * pcm_size(c->fi.format, 1);
	return PHI_DATA;
}

const phi_filter phi_aconv = {
	aconv_open, aconv_close, aconv_process,
	"audio-conv"
};
