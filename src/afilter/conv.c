/** phiola: audio converter
2019, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <afilter/pcm_convert.h>

#define errlog(trk, ...)  phi_errlog(core, NULL, trk, __VA_ARGS__)
#define dbglog(trk, ...)  phi_dbglog(core, NULL, trk, __VA_ARGS__)

extern const phi_core *core;

enum {
	CONV_OUTBUF_MSEC = 500,
};

struct aconv {
	uint state;
	uint out_samp_size;
	struct phi_af fi, fo;
	ffstr in;
	ffvec buf;
	uint off;
};

static void* aconv_open(phi_track *t)
{
	struct aconv *c = ffmem_new(struct aconv);
	return c;
}

static void aconv_close(void *ctx, phi_track *t)
{
	struct aconv *c = ctx;
	ffmem_alignfree(c->buf.ptr);
	ffmem_free(c);
}

static const ushort pcm_fmt[] = {
	PHI_PCM_FLOAT32,
	PHI_PCM_FLOAT64,
	PHI_PCM_16,
	PHI_PCM_24,
	PHI_PCM_24_4,
	PHI_PCM_32,
	PHI_PCM_8,
};
static inline char* af_print(const struct phi_af *af, char *buf, ffsize cap, const char **fmts)
{
	int r = ffarrint16_find(pcm_fmt, FF_COUNT(pcm_fmt), af->format);
	r = ffs_format_r0(buf, cap - 1, "%s/%u/%u/%s"
		, fmts[r], af->rate, af->channels, (af->interleaved) ? "i" : "ni");
	buf[r] = '\0';
	return buf;
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

	static const char* fmtstr[] = {
		"float32",
		"float64",
		"int16",
		"int24",
		"int24-4",
		"int32",
		"int8",
	};
	char bufi[100], bufo[100];

	core->conf.log(core->conf.log_obj, level, NULL, t, "audio conversion%s: %s -> %s"
		, unsupp, af_print(in, bufi, 100, fmtstr), af_print(out, bufo, 100, fmtstr));
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
	c->fi = t->aconv.in;
	c->fo = t->aconv.out;

	ffsize cap;
	const struct phi_af *in = &c->fi;
	const struct phi_af *out = &c->fo;

	if (in->rate != out->rate) {
		return PHI_ERR;
	}

	if (c->fi.channels > 8)
		return PHI_ERR;

	int r = pcm_convert(&c->fo, NULL, &c->fi, NULL, 0);
	log_pcmconv(r, &c->fi, &c->fo, t);
	if (r != 0)
		return PHI_ERR;

	uint out_ch = c->fo.channels & PHI_PCM_CHMASK;
	c->out_samp_size = pcm_size(c->fo.format, out_ch);
	cap = msec_to_samples(CONV_OUTBUF_MSEC, c->fo.rate) * c->out_samp_size;
	uint n = cap;
	if (!c->fo.interleaved)
		n = sizeof(void*) * out_ch + cap;
	c->buf.ptr = ffmem_align(n, 16);
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
