/** phiola: AAC encode
2016, Simon Zolin */

#include <fdk-aac/fdk-aac-phi.h>

struct aac_enc {
	uint state;
	uint64 total_in, total_out;
	struct phi_af fmt;
	ffstr in;
	fdkaac_encoder *enc;
	fdkaac_conf info;
	void *buf;
	const short *pcm;
	uint pcmlen; // PCM data length in bytes
};

static void* aacw_create(phi_track *t)
{
	struct aac_enc *a = phi_track_allocT(t, struct aac_enc);
	return a;
}

static void aacw_free(struct aac_enc *a, phi_track *t)
{
	if (a->enc)
		fdkaac_encode_free(a->enc);
	ffmem_free(a->buf);
	phi_track_free(t, a);
}

static int aac_enc_init(struct aac_enc *a, phi_track *t, ffstr *out)
{
	int q = (t->conf.aac.quality) ? t->conf.aac.quality : 5;
	if (q > 5 && q < 8000)
		q *= 1000;
	a->info.quality = q;

	a->info.aot = AAC_LC;
	switch (t->conf.aac.profile) {
	case 'h':
		a->info.aot = AAC_HE;  break;
	case 'H':
		a->info.aot = AAC_HEV2;  break;
	}

	a->info.afterburner = 1;
	a->info.bandwidth = t->conf.aac.bandwidth;
	a->info.channels = t->oaudio.format.channels;
	a->info.rate = t->oaudio.format.rate;

	int r;
	if ((r = fdkaac_encode_create(&a->enc, &a->info))) {
		errlog(t, "fdkaac_encode_create(): %s", fdkaac_encode_errstr(r));
		return PHI_ERR;
	}

	a->buf = ffmem_alloc(a->info.max_frame_size);

	t->oaudio.mp4_delay = a->info.enc_delay;
	t->oaudio.mp4_frame_samples = a->info.frame_samples;
	if (a->info.quality > 5)
		t->oaudio.mp4_bitrate = a->info.quality;

	ffstr asc = FFSTR_INITN(a->info.conf, a->info.conf_len);
	dbglog(t, "using bitrate %ubps, bandwidth %uHz, asc %*xb"
		, t->oaudio.mp4_bitrate, a->info.bandwidth, asc.len, asc.ptr);
	*out = asc;
	return 0;
}

static int aacw_encode(struct aac_enc *a, phi_track *t)
{
	int r;

	if (t->chain_flags & PHI_FFWD) {
		a->in = t->data_in;
		a->total_in += t->data_in.len;
		a->pcm = (void*)a->in.ptr,  a->pcmlen = a->in.len;
	}

	switch (a->state) {
	case 0:
		t->oaudio.conv_format.format = PHI_PCM_16;
		t->oaudio.conv_format.interleaved = 1;
		a->state = 1;
		return PHI_MORE;

	case 1:
		if (t->oaudio.format.format != PHI_PCM_16
			|| !t->oaudio.format.interleaved) {
			errlog(t, "input audio format not supported");
			return PHI_ERR;
		}
		a->fmt = t->oaudio.format;
		t->data_type = "AAC";

		if (aac_enc_init(a, t, &t->data_out))
			return PHI_ERR;
		a->state = 2;
		return PHI_DATA;
	}

	size_t n = a->pcmlen / (a->info.channels * sizeof(short));
	if (n == 0 && !(t->chain_flags & PHI_FFIRST))
		return PHI_MORE;

	for (;;) {
		r = fdkaac_encode(a->enc, a->pcm, &n, a->buf);
		if (r < 0) {
			errlog(t, "fdkaac_encode(): %s", fdkaac_encode_errstr(r));
			return PHI_ERR;
		}

		a->pcm += n * a->info.channels,  a->pcmlen -= n * a->info.channels * sizeof(short);

		if (r == 0) {
			if (t->chain_flags & PHI_FFIRST) {
				if (n != 0) {
					n = 0;
					continue;
				}
				verblog(t, "compression: %u%%", (int)FFINT_DIVSAFE(a->total_out * 100, a->total_in));
				return PHI_DONE;
			}
			return PHI_MORE;
		}
		break;
	}

	ffstr_set(&t->data_out, a->buf, r);
	dbglog(t, "encoded %L samples into %L bytes"
		, (a->in.len - a->pcmlen) / phi_af_size(&a->fmt), t->data_out.len);
	ffstr_set(&a->in, (void*)a->pcm, a->pcmlen);
	a->total_out += t->data_out.len;
	return PHI_DATA;
}

const struct phi_filter phi_aac_enc = {
	aacw_create, (void*)aacw_free, (void*)aacw_encode,
	"aac-encode"
};
