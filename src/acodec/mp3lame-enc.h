/** phiola: MPEG-Layer3 encode
2015, Simon Zolin */

#include <mp3lame/lame-phi.h>

enum MPG_E {
	MPG_EOK,
	MPG_ESYS,
	MPG_EFMT,
};

enum MPG_R {
	MPG_RWARN = -2,
	MPG_RERR,
	MPG_RHDR,
	MPG_RDATA,
	MPG_RMORE,
	MPG_RDONE,
};

struct mpeg_enc {
	uint state1;

	uint state;
	int err;
	lame *lam;
	struct phi_af fmt;

	size_t pcmlen;
	union {
	const short **pcm;
	const float **pcmf;
	const short *pcmi;
	};
	size_t pcmoff;
	uint samp_size;

	ffvec buf;
	size_t datalen;
	const void *data;
	uint off;

	uint fin :1;
};

static const char* mpeg_enc_errstr(struct mpeg_enc *m)
{
	switch (m->err) {
	case MPG_ESYS:
		return "";

	case MPG_EFMT:
		return "PCM format error";
	}

	return lame_errstr(m->err);
}

/**
@qual: 9..0(better) for VBR or 10..320 for CBR
Return enum MPG_E. */
static int mpg_create(struct mpeg_enc *m, phi_track *t, struct phi_af *af, int qual)
{
	int r;

	switch (af->format) {
	case PHI_PCM_16:
		break;

	case PHI_PCM_FLOAT32:
		if (!af->interleaved)
			break;
		// fallthrough

	default:
		af->format = PHI_PCM_16;
		m->err = MPG_EFMT;
		return MPG_EFMT;
	}

	struct lame_params conf = {
		.format = pcm_bits(af->format),
		.interleaved = (af->channels == 1) ? 0 : af->interleaved,
		.channels = af->channels,
		.rate = af->rate,
		.quality = qual,
	};
	if ((r = lame_create(&m->lam, &conf))) {
		m->err = r;
		return MPG_EFMT;
	}

	m->buf.cap = 125 * (8 * 1152) / 100 + 7200;
	m->buf.ptr = phi_track_alloc(t, m->buf.cap);
	m->fmt = *af;
	m->samp_size = pcm_size1(af);
	return MPG_EOK;
}

/**
Return enum MPG_R. */
static int mpeg_encode(struct mpeg_enc *m)
{
	enum { I_DATA, I_LAMETAG };
	size_t nsamples;
	int r = 0;

	switch (m->state) {
	case I_LAMETAG:
		r = lame_lametag(m->lam, m->buf.ptr, m->buf.cap);
		m->data = m->buf.ptr;
		m->datalen = ((uint)r <= m->buf.cap) ? r : 0;
		return MPG_RDONE;
	}

	for (;;) {

		nsamples = m->pcmlen / m->samp_size;
		nsamples = ffmin(nsamples, 8 * 1152);

		r = 0;
		if (nsamples != 0) {
			const void *pcm[2];
			if (m->fmt.interleaved)
				pcm[0] = (char*)m->pcmi + m->pcmoff * m->fmt.channels;
			else {
				for (uint i = 0;  i != m->fmt.channels;  i++) {
					pcm[i] = (char*)m->pcm[i] + m->pcmoff;
				}
			}
			r = lame_encode(m->lam, pcm, nsamples, m->buf.ptr, m->buf.cap);
			if (r < 0) {
				m->err = r;
				return MPG_RERR;
			}
			m->pcmoff += nsamples * pcm_bits(m->fmt.format)/8;
			m->pcmlen -= nsamples * m->samp_size;
		}

		if (r == 0) {
			if (m->pcmlen != 0)
				continue;

			if (!m->fin) {
				m->pcmoff = 0;
				return MPG_RMORE;
			}

			r = lame_encode(m->lam, NULL, 0, (char*)m->buf.ptr, m->buf.cap);
			if (r < 0) {
				m->err = r;
				return MPG_RERR;
			}
			m->state = I_LAMETAG;
		}

		m->data = m->buf.ptr;
		m->datalen = r;
		return MPG_RDATA;
	}
}

void* mpeg_enc_open(phi_track *t)
{
	if (!ffsz_eq(t->data_type, "pcm")) {
		errlog(t, "unsupported input data format: %s", t->data_type);
		return PHI_OPEN_ERR;
	}

	struct mpeg_enc *m = phi_track_allocT(t, struct mpeg_enc);
	t->audio.mp3_lametag = 0;
	return m;
}

void mpeg_enc_close(void *ctx, phi_track *t)
{
	struct mpeg_enc *m = ctx;
	phi_track_free(t, m->buf.ptr);
	lame_free(m->lam);
	phi_track_free(t, m);
}

int mpeg_enc_process(void *ctx, phi_track *t)
{
	struct mpeg_enc *m = ctx;
	int r;

	switch (m->state1) {
	case 0:
	case 1: {
		struct phi_af af = t->oaudio.format;
		int q = (t->conf.mp3.quality) ? t->conf.mp3.quality - 1 : 2;
		if ((r = mpg_create(m, t, &af, q))) {

			if (r == MPG_EFMT && m->state1 == 0) {
				t->oaudio.conv_format.format = af.format;
				m->state1 = 1;
				return PHI_MORE;
			}

			errlog(t, "mpg_create() failed: %s", mpeg_enc_errstr(m));
			return PHI_ERR;
		}

		t->data_type = "mpeg";
		m->state1 = 2;
	}
	}

	if (t->chain_flags & PHI_FFWD) {
		m->pcm = (void*)t->data_in.ptr;
		m->pcmlen = t->data_in.len;
	}

	for (;;) {
		r = mpeg_encode(m);
		switch (r) {

		case MPG_RDATA:
			goto data;

		case MPG_RMORE:
			if (!(t->chain_flags & PHI_FFIRST))
				return PHI_MORE;
			m->fin = 1;
			break;

		case MPG_RDONE:
			t->audio.mp3_lametag = 1;
			goto data;

		default:
			errlog(t, "mpeg_encode() failed: %s", mpeg_enc_errstr(m));
			return PHI_ERR;
		}
	}

data:
	ffstr_set(&t->data_out, m->data, m->datalen);
	dbglog(t, "output: %L bytes", m->datalen);
	return (r == MPG_RDONE) ? PHI_DONE : PHI_DATA;
}

const phi_filter phi_mpeg_enc = {
	mpeg_enc_open, mpeg_enc_close, mpeg_enc_process,
	"mpeg-encode"
};
