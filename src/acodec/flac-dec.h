/** phiola: FLAC decode
2018, Simon Zolin */

#include <avpack/base/flac.h>
#include <FLAC/FLAC-phi.h>

struct flac_dec {
	uint sample_size;
	uint64 pos;
	flac_decoder *dec;
	flac_conf info;
	ffstr in;
	size_t pcmlen;
	void *out[FLAC__MAX_CHANNELS];
};

static void flac_dec_free(void *ctx, phi_track *t)
{
	struct flac_dec *f = ctx;
	if (f->dec)
		flac_decode_free(f->dec);
	phi_track_free(t, f);
}

static void* flac_dec_create(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;
	if (t->data_in.len != sizeof(struct flac_streaminfo))
		return PHI_OPEN_ERR;

	int r;
	struct flac_dec *f = phi_track_allocT(t, struct flac_dec);

	const struct flac_streaminfo *si = (void*)t->data_in.ptr;
	t->data_in.len = 0;

	flac_conf info = {
		.bps = phi_af_bits(&t->audio.format),
		.channels = t->audio.format.channels,
		.rate = t->audio.format.rate,

		.min_blocksize = ffint_be_cpu16_ptr(si->minblock),
		.max_blocksize = ffint_be_cpu16_ptr(si->maxblock),
	};
	if ((r = flac_decode_init(&f->dec, &info))) {
		errlog(t, "flac_decode_init(): %s", flac_errstr(r));
		flac_dec_free(f, t);
		return PHI_OPEN_ERR;
	}

	f->info = info;
	f->sample_size = phi_af_size(&t->audio.format);
	t->data_type = "pcm";
	return f;
}

static inline void int_le_cpu24(void *p, uint n)
{
	u_char *o = (u_char*)p;
	o[0] = (u_char)n;
	o[1] = (u_char)(n >> 8);
	o[2] = (u_char)(n >> 16);
}

/** Convert data between 32bit integer and any other integer PCM format.
e.g. 16bit: "11 22 00 00" <-> "11 22" */
static int pcm_from32(const int **src, void **dst, uint dstbits, uint channels, uint samples)
{
	uint ic, i;
	union {
	char **pb;
	short **psh;
	} to;
	to.psh = (void*)dst;

	switch (dstbits) {
	case 8:
		for (ic = 0;  ic < channels;  ic++) {
			for (i = 0;  i < samples;  i++) {
				to.pb[ic][i] = (char)src[ic][i];
			}
		}
		break;

	case 16:
		for (ic = 0;  ic < channels;  ic++) {
			for (i = 0;  i < samples;  i++) {
				to.psh[ic][i] = (short)src[ic][i];
			}
		}
		break;

	case 24:
		for (ic = 0;  ic < channels;  ic++) {
			for (i = 0;  i < samples;  i++) {
				int_le_cpu24(&to.pb[ic][i * 3], src[ic][i]);
			}
		}
		break;

	default:
		return -1;
	}
	return 0;
}

static int flac_dec_decode(void *ctx, phi_track *t)
{
	struct flac_dec *f = ctx;
	int r;

	if (t->chain_flags & PHI_FFIRST) {
		return PHI_DONE;
	}

	if (t->chain_flags & PHI_FFWD) {
		if (!t->data_in.len)
			return PHI_MORE; // after we've read the stream info in flac_dec_create()
		if (t->audio.pos != ~0ULL)
			f->pos = t->audio.pos;

		f->in = t->data_in;
		f->pcmlen = t->audio.flac_samples;
		if (!f->pcmlen)
			f->pcmlen = f->info.min_blocksize; // OGG reader doesn't provide per-packet duration
	}

	const int **out;
	if ((r = flac_decode(f->dec, f->in.ptr, f->in.len, &out))) {
		warnlog(t, "flac_decode(): %s", flac_errstr(r));
		return PHI_MORE;
	}

	for (uint i = 0;  i < f->info.channels;  i++) {
		f->out[i] = (void*)out[i];
	}

	pcm_from32(out, f->out, f->info.bps, f->info.channels, f->pcmlen); // in-place conversion
	t->audio.pos = f->pos;
	f->pos += f->pcmlen;

	ffstr_set(&t->data_out, f->out, f->pcmlen * f->info.bps/8 * f->info.channels);
	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / f->sample_size, t->audio.pos);
	return PHI_OK;
}

const phi_filter phi_flac_dec = {
	flac_dec_create, flac_dec_free, flac_dec_decode,
	"flac-decode"
};
