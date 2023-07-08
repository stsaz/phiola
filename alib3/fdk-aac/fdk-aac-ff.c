/** FDK-AAC interface
2016, Simon Zolin */

#include "fdk-aac-ff.h"
#include <aacdecoder_lib.h>
#include <aacenc_lib.h>
#include <memory.h>


static const char* const dec_errs[] = {
	"AAC_DEC_OK: No error occured. Output buffer is valid and error free.",
	"",
	"AAC_DEC_OUT_OF_MEMORY: Heap returned NULL pointer. Output buffer is invalid.",
	"",
	"",
	"AAC_DEC_UNKNOWN: Error condition is of unknown reason, or from a another module. Output buffer is invalid.",
};

static const char* const dec_errs_2000[] = {
	"",
	"AAC_DEC_INVALID_HANDLE: The handle passed to the function call was invalid (NULL).",
	"AAC_DEC_UNSUPPORTED_AOT: The AOT found in the configuration is not supported.",
	"AAC_DEC_UNSUPPORTED_FORMAT: The bitstream format is not supported. ",
	"AAC_DEC_UNSUPPORTED_ER_FORMAT: The error resilience tool format is not supported.",
	"AAC_DEC_UNSUPPORTED_EPCONFIG: The error protection format is not supported.",
	"AAC_DEC_UNSUPPORTED_MULTILAYER: More than one layer for AAC scalable is not supported.",
	"AAC_DEC_UNSUPPORTED_CHANNELCONFIG: The channel configuration (either number or arrangement) is not supported.",
	"AAC_DEC_UNSUPPORTED_SAMPLINGRATE: The sample rate specified in the configuration is not supported.",
	"AAC_DEC_INVALID_SBR_CONFIG: The SBR configuration is not supported.",
	"AAC_DEC_SET_PARAM_FAIL: The parameter could not be set. Either the value was out of range or the parameter does not exist.",
	"AAC_DEC_NEED_TO_RESTART: The decoder needs to be restarted, since the requiered configuration change cannot be performed.",
	"AAC_DEC_OUTPUT_BUFFER_TOO_SMALL: The provided output buffer is too small.",
};

static const char* const dec_errs_4000[] = {
	"",
	"AAC_DEC_TRANSPORT_ERROR: The transport decoder encountered an unexpected error.",
	"AAC_DEC_PARSE_ERROR: Error while parsing the bitstream. Most probably it is corrupted, or the system crashed.",
	"AAC_DEC_UNSUPPORTED_EXTENSION_PAYLOAD: Error while parsing the extension payload of the bitstream. The extension payload type found is not supported.",
	"AAC_DEC_DECODE_FRAME_ERROR: The parsed bitstream value is out of range. Most probably the bitstream is corrupt, or the system crashed.",
	"AAC_DEC_CRC_ERROR: The embedded CRC did not match.",
	"AAC_DEC_INVALID_CODE_BOOK: An invalid codebook was signalled. Most probably the bitstream is corrupt, or the system crashed.",
	"AAC_DEC_UNSUPPORTED_PREDICTION: Predictor found, but not supported in the AAC Low Complexity profile. Most probably the bitstream is corrupt, or has a wrong format.",
	"AAC_DEC_UNSUPPORTED_CCE: A CCE element was found which is not supported. Most probably the bitstream is corrupt, or has a wrong format.",
	"AAC_DEC_UNSUPPORTED_LFE: A LFE element was found which is not supported. Most probably the bitstream is corrupt, or has a wrong format.",
	"AAC_DEC_UNSUPPORTED_GAIN_CONTROL_DATA: Gain control data found but not supported. Most probably the bitstream is corrupt, or has a wrong format.",
	"AAC_DEC_UNSUPPORTED_SBA: SBA found, but currently not supported in the BSAC profile.",
	"AAC_DEC_TNS_READ_ERROR: Error while reading TNS data. Most probably the bitstream is corrupt or the system crashed.",
	"AAC_DEC_RVLC_ERROR: Error while decoding error resillient data.",
};

const char* fdkaac_decode_errstr(int e)
{
	int n;
	const char* const *ee;
	e = -e;
	switch (e & 0xf000) {
	case 0x0000:
		ee = dec_errs;
		n = sizeof(dec_errs) / sizeof(*dec_errs);
		break;

	case 0x2000:
		ee = dec_errs_2000;
		n = sizeof(dec_errs_2000) / sizeof(*dec_errs_2000);
		break;

	case 0x4000:
		ee = dec_errs_4000;
		n = sizeof(dec_errs_4000) / sizeof(*dec_errs_4000);
		break;

	default:
		return "";
	}
	if ((e & 0xfff) >= n)
		return "";
	return ee[e & 0xfff];
}

int fdkaac_decode_open(fdkaac_decoder **pdec, const char *conf, size_t len)
{
	int r;
	HANDLE_AACDECODER dec;
	if (NULL == (dec = aacDecoder_Open(TT_MP4_RAW, 1))) {
		r = AAC_DEC_OUT_OF_MEMORY;
		goto end;
	}

	unsigned char *bdata = (void*)conf;
	unsigned int ulen = len;
	if (0 != (r = aacDecoder_ConfigRaw(dec, &bdata, &ulen))) {
		aacDecoder_Close(dec);
		goto end;
	}

	/*if (0 != (r = aacDecoder_SetParam(dec, AAC_PCM_OUTPUT_INTERLEAVED, 0))) {
		aacDecoder_Close(dec);
		goto end;
	}*/

	*pdec = dec;
	r = 0;

end:
	return -r;
}

void fdkaac_decode_free(fdkaac_decoder *dec)
{
	aacDecoder_Close(dec);
}

int fdkaac_decode(fdkaac_decoder *dec, const char *data, size_t len, short *pcm)
{
	int r;
	unsigned char *bdata = (void*)data;
	unsigned int ulen = len, valid = len;
	if (0 != (r = aacDecoder_Fill(dec, &bdata, &ulen, &valid)))
		return -r;

	r = aacDecoder_DecodeFrame(dec, pcm, AAC_MAXCHANNELS * AAC_MAXFRAMESAMPLES, 0);
	if (r == AAC_DEC_NOT_ENOUGH_BITS)
		return 0;
	else if (r != 0)
		return -r;

	const CStreamInfo *si = aacDecoder_GetStreamInfo(dec);
	return si->frameSize;
}

int fdkaac_frameinfo(fdkaac_decoder *dec, fdkaac_info *info)
{
	const CStreamInfo *si = aacDecoder_GetStreamInfo(dec);
	info->aot = AAC_LC;
	info->channels = si->numChannels;
	info->rate = si->sampleRate;
	info->bitrate = si->bitRate;
	if (si->flags & AC_SBR_PRESENT) {
		info->aot = AAC_HE;
		if (si->flags & AC_PS_PRESENT)
			info->aot = AAC_HEV2;
	}
	return 0;
}


static const char* const enc_errs_20[] = {
	"AACENC_INVALID_HANDLE: Handle passed to function call was invalid.",
	"AACENC_MEMORY_ERROR: Memory allocation failed.",
	"AACENC_UNSUPPORTED_PARAMETER: Parameter not available.",
	"AACENC_INVALID_CONFIG: Configuration not provided.",
};

static const char* const enc_errs_40[] = {
	"AACENC_INIT_ERROR: General initialization error.",
	"AACENC_INIT_AAC_ERROR: AAC library initialization error.",
	"AACENC_INIT_SBR_ERROR: SBR library initialization error.",
	"AACENC_INIT_TP_ERROR: Transport library initialization error.",
	"AACENC_INIT_META_ERROR: Meta data library initialization error.",
};

static const char* const enc_errs_60[] = {
	"AACENC_ENCODE_ERROR: The encoding process was interrupted by an unexpected error.",
};

const char* fdkaac_encode_errstr(int e)
{
	const char* const *ee;
	int n;
	e = -e;
	switch (e & 0xfff0) {
	case 0x20:
		ee = enc_errs_20;
		n = sizeof(enc_errs_20) / sizeof(*enc_errs_20);
		break;

	case 0x40:
		ee = enc_errs_40;
		n = sizeof(enc_errs_40) / sizeof(*enc_errs_40);
		break;

	case 0x60:
		ee = enc_errs_60;
		n = sizeof(enc_errs_60) / sizeof(*enc_errs_60);
		break;

	default:
		return "";
	}
	if ((e & 0x0f) > n)
		return "";
	return ee[e & 0x0f];
}


struct fdkaac_encoder {
	HANDLE_AACENCODER enc;
	int channels;
};

int fdkaac_encode_create(fdkaac_encoder **penc, fdkaac_conf *conf)
{
	HANDLE_AACENCODER enc;
	int r;
	fdkaac_encoder *a;

	if (NULL == (a = calloc(1, sizeof(fdkaac_encoder)))) {
		r = AACENC_MEMORY_ERROR;
		goto end;
	}
	a->channels = conf->channels;

	if (AACENC_OK != (r = aacEncOpen(&enc, 0, conf->channels)))
		goto end;
	a->enc = enc;

	if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_AOT, conf->aot)))
		goto end;
	if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_SAMPLERATE, conf->rate)))
		goto end;
	if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_CHANNELMODE, conf->channels))) //enum CHANNEL_MODE
		goto end;
	if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_CHANNELORDER, 1 /*CH_ORDER_WAV*/)))
		goto end;

	if (conf->quality >= 1 && conf->quality <= 5) {
		if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_BITRATEMODE, conf->quality /*AACENC_BR_MODE_VBR_*/)))
			goto end;
	} else {
		if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_BITRATE, conf->quality)))
			goto end;
	}

	if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_BANDWIDTH, conf->bandwidth)))
		goto end;
	if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_AFTERBURNER, conf->afterburner)))
		goto end;
	if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_TRANSMUX, TT_MP4_RAW)))
		goto end;
	if (AACENC_OK != (r = aacEncoder_SetParam(enc, AACENC_SIGNALING_MODE, 1 /*Explicit backward compatible signaling*/ )))
		goto end;

	if (AACENC_OK != (r = aacEncEncode(enc, NULL, NULL, NULL, NULL)))
		goto end;

	AACENC_InfoStruct info;
	if (AACENC_OK != (r = aacEncInfo(enc, &info)))
		goto end;
	memcpy(conf->conf, info.confBuf, info.confSize);
	conf->conf_len = info.confSize;
	conf->frame_samples = info.frameLength;
	conf->max_frame_size = info.maxOutBufBytes;
	conf->enc_delay = info.encoderDelay;
	if (-1 != (r = aacEncoder_GetParam(enc, AACENC_BITRATE)))
		conf->quality = r;
	conf->bandwidth = aacEncoder_GetParam(enc, AACENC_BANDWIDTH);

	*penc = a;
	return 0;

end:
	fdkaac_encode_free(a);
	return -r;
}

void fdkaac_encode_free(fdkaac_encoder *a)
{
	aacEncClose(&a->enc);
	free(a);
}

int fdkaac_encode(fdkaac_encoder *a, const short *audio, size_t *samples, char *data)
{
	int r;
	AACENC_BufDesc in, out;
	AACENC_InArgs in_args = {0};
	AACENC_OutArgs out_args = {0};

	in.numBufs = 1;
	in.bufs = (void**)&audio;
	int id = IN_AUDIO_DATA;
	in.bufferIdentifiers = &id;
	int size = *samples * a->channels * sizeof(short);
	in.bufSizes = &size;
	int elsize = sizeof(short);
	in.bufElSizes = &elsize;

	out.numBufs = 1;
	out.bufs = (void*)&data;
	int out_id = OUT_BITSTREAM_DATA;
	out.bufferIdentifiers = &out_id;
	int out_size = 768 * a->channels;
	out.bufSizes = &out_size;
	int out_elsize = 1;
	out.bufElSizes = &out_elsize;

	in_args.numInSamples = (*samples != 0) ? *samples * a->channels : -1;
	r = aacEncEncode(a->enc, &in, &out, &in_args, &out_args);
	if (r == AACENC_OK) {}
	else if (r == AACENC_ENCODE_EOF)
		return 0;
	else
		return -r;

	*samples = out_args.numInSamples / a->channels;
	return out_args.numOutBytes;
}
