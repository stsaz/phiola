/** phiola: FLAC encode
2015, Simon Zolin */

#include <avpack/base/flac.h>
#include <FLAC/FLAC-phi.h>

struct flac_enc {
	uint state;
	flac_encoder *enc;
	struct flac_info info;
	size_t pcmlen;
	const void **pcm;
	int* pcm32[FLAC__MAX_CHANNELS];
	uint cap_pcm32, off_pcm, off_pcm32;
	ffvec obuf;
};

static void* flac_enc_create(phi_track *t)
{
	if (t->data_type != PHI_AC_PCM) {
		errlog(t, "unsupported input data format: %u", t->data_type);
		return PHI_OPEN_ERR;
	}

	struct flac_enc *f = phi_track_allocT(t, struct flac_enc);
	return f;
}

static void flac_enc_free(struct flac_enc *f, phi_track *t)
{
	ffvec_free(&f->obuf);
	if (f->enc)
		flac_encode_free(f->enc);
	phi_track_free(t, f);
}

static int flac_format_supported(struct phi_af *f)
{
	switch (f->format) {
	case PHI_PCM_8:
	case PHI_PCM_16:
	case PHI_PCM_24:
		return 1;
	}
	return 0;
}

static int flac_enc_init(struct flac_enc *f, phi_track *t)
{
	flac_conf conf = {
		.bps = phi_af_bits(&t->oaudio.format),
		.channels = t->oaudio.format.channels,
		.rate = t->oaudio.format.rate,
		.level = 6,
	};
	int r;
	if ((r = flac_encode_init(&f->enc, &conf))) {
		errlog(t, "ffflac_create(): %s", flac_errstr(r));
		return PHI_ERR;
	}

	f->info.bits = conf.bps;
	f->info.channels = conf.channels;
	f->info.sample_rate = conf.rate;

	flac_conf info;
	flac_encode_info(f->enc, &info);
	f->info.minblock = info.min_blocksize;
	f->info.maxblock = info.max_blocksize;

	ffvec_realloc(&f->obuf, (f->info.minblock + 1) * sizeof(int) * f->info.channels, 1);
	for (uint i = 0;  i < f->info.channels;  i++) {
		f->pcm32[i] = (void*)(f->obuf.ptr + (f->info.minblock + 1) * sizeof(int) * i);
	}
	f->cap_pcm32 = f->info.minblock + 1;

	t->oaudio.flac_vendor = flac_vendor();
	t->data_type = PHI_AC_FLAC;
	return 0;
}

static int int_le_cpu24s_ptr(const void *p)
{
	const u_char *b = (u_char*)p;
	uint n = ((uint)b[2] << 16) | ((uint)b[1] << 8) | b[0];
	if (n & 0x00800000)
		n |= 0xff000000;
	return n;
}

/** Convert data between 32bit integer and any other integer PCM format.
e.g. 16bit: "11 22 00 00" <-> "11 22" */
static int pcm_to32(int **dst, const void **src, uint srcbits, uint channels, uint samples)
{
	uint ic, i;
	union {
	char **pb;
	short **psh;
	} from;
	from.psh = (void*)src;

	switch (srcbits) {
	case 8:
		for (ic = 0;  ic < channels;  ic++) {
			for (i = 0;  i < samples;  i++) {
				dst[ic][i] = from.pb[ic][i];
			}
		}
		break;

	case 16:
		for (ic = 0;  ic < channels;  ic++) {
			for (i = 0;  i < samples;  i++) {
				dst[ic][i] = from.psh[ic][i];
			}
		}
		break;

	case 24:
		for (ic = 0;  ic < channels;  ic++) {
			for (i = 0;  i < samples;  i++) {
				dst[ic][i] = int_le_cpu24s_ptr(&from.pb[ic][i * 3]);
			}
		}
		break;

	default:
		return -1;
	}

	return 0;
}

/*
An input sample must be within 32-bit container.
To encode a frame libFLAC needs NBLOCK+1 input samples.
flac_encode() returns a frame with NBLOCK encoded samples,
 so 1 sample always stays cached in libFLAC until we explicitly flush output data.
*/
static int flac_enc_encode(struct flac_enc *f, phi_track *t)
{
	int r;
	uint samples, sample_size, blksize;
	char *data;
	enum { I_CONV, I_INIT, I_ENC, I_DONE };

	if (t->chain_flags & PHI_FFWD) {
		f->pcm = (const void**)t->data_in.ptr;
		f->pcmlen = t->data_in.len;
	}

	switch (f->state) {
	case I_CONV:
	case I_INIT:
		if (!(r = flac_format_supported(&t->oaudio.format))
			|| t->oaudio.format.interleaved) {

			if (f->state == I_CONV) {
				f->state = I_INIT;
				if (!r)
					t->oaudio.conv_format.format = PHI_PCM_24;
				t->oaudio.conv_format.interleaved = 0;
				return PHI_MORE;
			}
			errlog(t, "format not supported");
			return PHI_ERR;
		}

		if (flac_enc_init(f, t))
			return PHI_ERR;
		ffstr_set(&t->data_out, (void*)&f->info, sizeof(f->info));
		f->state = I_ENC;
		return PHI_DATA;

	case I_DONE: {
		flac_conf info = {};
		flac_encode_info(f->enc, &info);
		f->info.minblock = info.min_blocksize;
		f->info.maxblock = info.max_blocksize;
		f->info.minframe = info.min_framesize;
		f->info.maxframe = info.max_framesize;
		ffmem_copy(f->info.md5, info.md5, sizeof(f->info.md5));
		ffstr_set(&t->data_out, (void*)&f->info, sizeof(f->info));
		return PHI_DONE;
	}
	}

	sample_size = f->info.bits/8 * f->info.channels;
	samples = ffmin(f->pcmlen / sample_size - f->off_pcm, f->cap_pcm32 - f->off_pcm32);

	if (samples == 0 && !(t->chain_flags & PHI_FFIRST)) {
		f->off_pcm = 0;
		return PHI_MORE;
	}

	if (samples != 0) {
		const void* src[FLAC__MAX_CHANNELS];
		int* dst[FLAC__MAX_CHANNELS];

		for (uint i = 0;  i != f->info.channels;  i++) {
			src[i] = (char*)f->pcm[i] + f->off_pcm * f->info.bits/8;
			dst[i] = f->pcm32[i] + f->off_pcm32;
		}

		r = pcm_to32(dst, src, f->info.bits, f->info.channels, samples);
		FF_ASSERT(!r);

		f->off_pcm += samples;
		f->off_pcm32 += samples;
		if (!(f->off_pcm32 == f->cap_pcm32 || (t->chain_flags & PHI_FFIRST))) {
			f->off_pcm = 0;
			return PHI_MORE;
		}
	}

	samples = f->off_pcm32;
	f->off_pcm32 = 0;
	r = flac_encode(f->enc, (const int**)f->pcm32, &samples, &data);
	if (r < 0) {
		errlog(t, "flac_encode(): %s", flac_errstr(r));
		return PHI_ERR;
	}

	blksize = f->info.minblock;
	if (r == 0 && (t->chain_flags & PHI_FFIRST)) {
		samples = 0;
		r = flac_encode(f->enc, (const int**)f->pcm32, &samples, &data);
		if (r < 0) {
			errlog(t, "flac_encode(): %s", flac_errstr(r));
			return PHI_ERR;
		}
		blksize = samples;
		f->state = I_DONE;
	}

	FF_ASSERT(r != 0);
	FF_ASSERT(samples == f->cap_pcm32 || (t->chain_flags & PHI_FFIRST));

	if (f->cap_pcm32 == f->info.minblock + 1)
		f->cap_pcm32 = f->info.minblock;

	t->oaudio.flac_frame_samples = blksize;
	ffstr_set(&t->data_out, data, r);
	dbglog(t, "output: %L bytes", t->data_out.len);
	return PHI_DATA;
}

const phi_filter phi_flac_enc = {
	flac_enc_create, (void*)flac_enc_free, (void*)flac_enc_encode,
	"flac-encode"
};
