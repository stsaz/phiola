/** phiola: AAC encode
2016, Simon Zolin */

#include <acodec/alib3-bridge/aac-enc-if.h>

struct aac_enc {
	uint state;
	uint64 total_in, total_out;
	struct phi_af fmt;
	ffstr in;
	ffaac_enc aac;
};

static int aac_aot_profile(char profile)
{
	switch (profile) {
	case 'l':
	case 0:
		return AAC_LC;
	case 'h': return AAC_HE;
	case 'H': return AAC_HEV2;
	}
	return 0;
}

static void* aacw_create(phi_track *t)
{
	struct aac_enc *a = phi_track_allocT(t, struct aac_enc);
	return a;
}

static void aacw_free(struct aac_enc *a, phi_track *t)
{
	ffaac_enc_close(&a->aac);
	phi_track_free(t, a);
}

static int aacw_encode(struct aac_enc *a, phi_track *t)
{
	int r;

	if (t->chain_flags & PHI_FFWD) {
		a->in = t->data_in;
		a->total_in += t->data_in.len;
		a->aac.pcm = (void*)a->in.ptr,  a->aac.pcmlen = a->in.len;
		if (t->chain_flags & PHI_FFIRST)
			a->aac.fin = 1;
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

		int qual = (t->conf.aac.quality) ? t->conf.aac.quality : 5;
		if (qual > 5 && qual < 8000)
			qual *= 1000;

		a->aac.info.aot = aac_aot_profile(t->conf.aac.profile);
		a->aac.info.afterburner = 1;
		a->aac.info.bandwidth = t->conf.aac.bandwidth;
		if (0 != ffaac_create(&a->aac, &a->fmt, qual)) {
			errlog(t, "ffaac_create(): %s", ffaac_enc_errstr(&a->aac));
			return PHI_ERR;
		}

		t->oaudio.mp4_delay = a->aac.info.enc_delay;
		t->oaudio.mp4_frame_samples = ffaac_enc_frame_samples(&a->aac);
		if (a->aac.info.quality > 5)
			t->oaudio.mp4_bitrate = a->aac.info.quality;
		ffstr asc = ffaac_enc_conf(&a->aac);
		dbglog(t, "using bitrate %ubps, bandwidth %uHz, asc %*xb"
			, t->oaudio.mp4_bitrate, a->aac.info.bandwidth, asc.len, asc.ptr);

		t->data_out = asc;
		a->state = 2;
		return PHI_DATA;
	}

	r = ffaac_encode(&a->aac);

	switch (r) {
	case FFAAC_RDATA:
		break;

	case FFAAC_RDONE:
		verblog(t, "compression: %u%%", (int)FFINT_DIVSAFE(a->total_out * 100, a->total_in));
		return PHI_DONE;

	case FFAAC_RMORE:
		return PHI_MORE;

	case FFAAC_RERR:
		errlog(t, "ffaac_encode(): %s", ffaac_enc_errstr(&a->aac));
		return PHI_ERR;
	}

	dbglog(t, "encoded %L samples into %L bytes"
		, (a->in.len - a->aac.pcmlen) / phi_af_size(&a->fmt), a->aac.datalen);
	ffstr_set(&a->in, (void*)a->aac.pcm, a->aac.pcmlen);
	ffstr_set(&t->data_out, a->aac.data, a->aac.datalen);
	a->total_out += t->data_out.len;
	return PHI_DATA;
}

const struct phi_filter phi_aac_enc = {
	aacw_create, (void*)aacw_free, (void*)aacw_encode,
	"aac-encode"
};
