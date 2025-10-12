/** phiola: AAC decode
2016, Simon Zolin */

#include <afilter/pcm.h>
#include <fdk-aac/fdk-aac-phi.h>

struct aac_in {
	uint state;
	ffvec cache;
	uint sample_rate;
	uint frame;
	uint avg_bitrate;
	fdkaac_decoder *dec;
	ffstr in;
	fdkaac_info info;
	struct phi_af fmt;
	void *pcmbuf;
	uint64 pos;
	uint contr_sample_rate;
	uint rate_mul;
};

enum { DETECT_FRAMES = 8, }; //# of frames to detect real audio format

static void* aac_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct aac_in *a = phi_track_allocT(t, struct aac_in);
	a->contr_sample_rate = t->audio.format.rate;
	int r;
	if ((r = fdkaac_decode_open(&a->dec, t->data_in.ptr, t->data_in.len))) {
		errlog(t, "ffaac_open(): %s", fdkaac_decode_errstr(r));
		phi_track_free(t, a);
		return PHI_OPEN_ERR;
	}

	a->fmt.format = PHI_PCM_16;
	a->fmt.rate = a->contr_sample_rate;
	a->fmt.channels = t->audio.format.channels;
	a->pcmbuf = ffmem_alloc(AAC_MAXFRAMESAMPLES * pcm_size(PHI_PCM_16, AAC_MAXCHANNELS));
	a->rate_mul = 1;
	t->data_in.len = 0;
	t->audio.format.format = a->fmt.format;
	t->audio.format.interleaved = 1;
	a->sample_rate = t->audio.format.rate;
	t->data_type = "pcm";
	t->audio.decoder = "AAC";
	return a;
}

static void aac_close(struct aac_in *a, phi_track *t)
{
	if (a->dec)
		fdkaac_decode_free(a->dec);
	ffmem_free(a->pcmbuf);
	ffvec_free(&a->cache);
	phi_track_free(t, a);
}

/** Dynamic mean value with weight. */
#define ffint_mean_dyn(mean, weight, add) \
	(((mean) * (weight) + (add)) / ((weight) + 1))

/*
AAC audio format detection:
. gather decoded frames in cache
. when decoder notifies about changed audio format:
 . set new audio format
 . clear cached data
. when the needed number of frames has been processed, start returning data
If format changes again in the future, the change won't be handled.
*/
static int aac_decode(void *ctx, phi_track *t)
{
	struct aac_in *a = ctx;
	int r;
	uint new_format;
	ffstr out;
	enum { R_CACHE, R_CACHE_DATA, R_CACHE_DONE, R_PASS };

	if (t->chain_flags & PHI_FFWD) {
		a->in = t->data_in;
		if (t->audio.pos != ~0ULL)
			a->pos = t->audio.pos * a->rate_mul;
	}

	for (;;) {

		r = fdkaac_decode(a->dec, a->in.ptr, a->in.len, a->pcmbuf);
		if (r == 0) {
			if (!(t->chain_flags & PHI_FFIRST))
				return PHI_MORE;
			else if (a->cache.len == 0)
				return PHI_DONE;
			a->state = R_CACHE_DATA;

		} else if (r < 0) {
			warnlog(t, "fdkaac_decode(): (%xu) %s", r, fdkaac_decode_errstr(r));
			return PHI_MORE;

		} else {
			a->in.len = 0;
			new_format = 0;
			fdkaac_frameinfo(a->dec, &a->info);
			if (a->fmt.rate != a->info.rate
				|| a->fmt.channels != a->info.channels) {
				new_format = 1;
				a->fmt.channels = a->info.channels;
				a->fmt.rate = a->info.rate;
				if (a->contr_sample_rate != 0)
					a->rate_mul = a->info.rate / a->contr_sample_rate;
			}

			ffstr_set(&out, a->pcmbuf, r * pcm_size(a->fmt.format, a->info.channels));
			const fdkaac_info *inf = &a->info;
			dbglog(t, "decoded %u samples @%U  aot:%u rate:%u chan:%u bitrate:%u"
				, out.len / phi_af_size(&a->fmt), a->pos
				, inf->aot, inf->rate, inf->channels, inf->bitrate);
			a->pos += r;
		}

		switch (a->state) {
		case R_CACHE:
			if (new_format) {
				struct phi_af *fmt = &t->audio.format;
				const fdkaac_info *inf = &a->info;
				dbglog(t, "overriding audio configuration: %u/%u -> %u/%u"
					, fmt->rate, fmt->channels
					, inf->rate, inf->channels);
				if (fmt->rate != inf->rate) {
					if (t->audio.total != ~0ULL) {
						t->audio.total *= a->rate_mul;
					}
				}
				a->sample_rate = fmt->rate = inf->rate;
				fmt->channels = inf->channels;
				a->cache.len = 0;
			}
			a->avg_bitrate = ffint_mean_dyn(a->avg_bitrate, a->frame, a->info.bitrate);
			ffvec_addstr(&a->cache, &out);
			if (++a->frame != DETECT_FRAMES)
				continue;
			// fallthrough

		case R_CACHE_DATA:
			if (t->audio.total == ~0ULL && t->input.size != ~0ULL)
				t->audio.total = t->input.size * 8 * a->sample_rate / a->avg_bitrate;

			if (!t->audio.bitrate)
				t->audio.bitrate = a->avg_bitrate;

			switch (a->info.aot) {
			case AAC_LC:
				t->audio.decoder = "AAC-LC"; break;
			case AAC_HE:
				t->audio.decoder = "HE-AAC"; break;
			case AAC_HEV2:
				t->audio.decoder = "HE-AACv2"; break;
			}

			t->audio.pos = a->pos - a->cache.len / phi_af_size(&a->fmt);
			t->audio.pos = ffmax((ffint64)t->audio.pos, 0);
			t->data_out = *(ffstr*)&a->cache;
			a->state = R_CACHE_DONE;
			return PHI_DATA;

		case R_CACHE_DONE:
			ffvec_free(&a->cache);
			a->state = R_PASS;
			break;
		}

		break;
	}

	t->data_out = out;
	t->audio.pos = a->pos - out.len / phi_af_size(&a->fmt);
	return PHI_DATA;
}

const phi_filter phi_aac_dec = {
	aac_open, (void*)aac_close, (void*)aac_decode,
	"aac-decode"
};
