/** libopus interface
2016, Simon Zolin */

#include "opus-ff.h"
#include <opus.h>

const char* opus_errstr(int e)
{
	return opus_strerror(e);
}


int opus_decode_init(opus_ctx **pc, opus_conf *conf)
{
	int e;
	OpusDecoder *c = opus_decoder_create(48000, conf->channels, &e);
	if (e != OPUS_OK)
		return e;
	*pc = c;
	return 0;
}

void opus_decode_free(opus_ctx *oc)
{
	OpusDecoder *c = oc;
	opus_decoder_destroy(c);
}

int opus_decode_f(opus_ctx *oc, const void *packet, unsigned int len, float *pcm)
{
	OpusDecoder *c = oc;
	int r = opus_decode_float(c, packet, len, pcm, OPUS_BUFLEN(48000), 0);
	return r;
}

void opus_decode_reset(opus_ctx *c)
{
	opus_decoder_ctl(c, OPUS_RESET_STATE);
}


const char* opus_vendor(void)
{
	return opus_get_version_string();
}

static int bandwidth_val(unsigned int bandwidth)
{
	int val = -1;
	switch (bandwidth) {
	case 4:
		val = OPUS_BANDWIDTH_NARROWBAND;
		break;
	case 6:
		val = OPUS_BANDWIDTH_MEDIUMBAND;
		break;
	case 8:
		val = OPUS_BANDWIDTH_WIDEBAND;
		break;
	case 12:
		val = OPUS_BANDWIDTH_SUPERWIDEBAND;
		break;
	case 20:
		val = OPUS_BANDWIDTH_FULLBAND;
		break;
	}
	return val;
}

int opus_encode_create(opus_ctx **pc, opus_encode_conf *conf)
{
	int e;
	unsigned int rate = (conf->sample_rate != 0) ? conf->sample_rate : 48000;
	int app = (conf->application == OPUS_VOIP) ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO;
	OpusEncoder *c = opus_encoder_create(rate, conf->channels, app, &e);
	if (e != OPUS_OK)
		return e;

	if (conf->bitrate != 0
		&& OPUS_OK != (e = opus_encoder_ctl(c, OPUS_SET_BITRATE(conf->bitrate))))
		goto fail;

	if (conf->complexity != 0
		&& OPUS_OK != (e = opus_encoder_ctl(c, OPUS_SET_COMPLEXITY(conf->complexity - 1))))
		goto fail;

	if (conf->bandwidth != 0
		&& OPUS_OK != (e = opus_encoder_ctl(c, OPUS_SET_MAX_BANDWIDTH(bandwidth_val(conf->bandwidth)))))
		goto fail;

	int lookahead;
	if (OPUS_OK != (e = opus_encoder_ctl(c, OPUS_GET_LOOKAHEAD(&lookahead))))
		goto fail;
	conf->preskip = lookahead;

	*pc = c;
	return 0;

fail:
	opus_encoder_destroy(c);
	return e;
}

void opus_encode_free(opus_ctx *oc)
{
	OpusEncoder *c = oc;
	opus_encoder_destroy(c);
}

int opus_encode_f(opus_ctx *oc, const float *pcm, int samples, void *pkt)
{
	OpusEncoder *c = oc;
	int r;
	r = opus_encode_float(c, pcm, samples, pkt, OPUS_MAX_PKT);
	return r;
}
