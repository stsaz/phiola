/** phiola: AAC decode
2016, Simon Zolin */

#include <acodec/alib3-bridge/aac-dec-if.h>

struct aac_in {
	ffaac aac;
	uint state;
	ffvec cache;
	uint sample_rate;
	uint frnum;
	uint br;
};

enum { DETECT_FRAMES = 32, }; //# of frames to detect real audio format

static void* aac_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct aac_in *a = phi_track_allocT(t, struct aac_in);
	a->aac.contr_samprate = t->audio.format.rate;
	if (0 != ffaac_open(&a->aac, t->audio.format.channels, t->data_in.ptr, t->data_in.len)) {
		errlog(t, "ffaac_open(): %s", ffaac_errstr(&a->aac));
		phi_track_free(t, a);
		return PHI_OPEN_ERR;
	}
	t->data_in.len = 0;
	t->audio.format.format = a->aac.fmt.format;
	t->audio.format.interleaved = 1;
	a->sample_rate = t->audio.format.rate;
	t->data_type = "pcm";
	t->audio.decoder = "AAC";
	return a;
}

static void aac_close(struct aac_in *a, phi_track *t)
{
	ffaac_close(&a->aac);
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
	uint64 apos = 0;
	ffstr out;
	enum { R_CACHE, R_CACHE_DATA, R_CACHE_DONE, R_PASS };

	if (t->chain_flags & PHI_FFWD) {
		ffaac_input(&a->aac, t->data_in.ptr, t->data_in.len, t->audio.pos);
		t->data_in.len = 0;
	}

	for (;;) {

		r = ffaac_decode(&a->aac, &out, &apos);
		if (r == FFAAC_RERR) {
			warnlog(t, "ffaac_decode(): (%xu) %s", a->aac.err, ffaac_errstr(&a->aac));
			return PHI_MORE;

		} else if (r == FFAAC_RMORE) {
			if (!(t->chain_flags & PHI_FFIRST))
				return PHI_MORE;
			else if (a->cache.len == 0) {
				return PHI_DONE;
			}
			a->state = R_CACHE_DATA;

		} else {
			const fdkaac_info *inf = &a->aac.info;
			dbglog(t, "decoded %u samples @%U  aot:%u rate:%u chan:%u br:%u"
				, out.len / phi_af_size(&a->aac.fmt), apos
				, inf->aot, inf->rate, inf->channels, inf->bitrate);
		}

		switch (a->state) {
		case R_CACHE:
			if (r == FFAAC_RDATA_NEWFMT) {
				struct phi_af *fmt = &t->audio.format;
				fdkaac_info *inf = &a->aac.info;
				dbglog(t, "overriding audio configuration: %u/%u -> %u/%u"
					, fmt->rate, fmt->channels
					, inf->rate, inf->channels);
				if (fmt->rate != inf->rate) {
					if (t->audio.total != 0) {
						t->audio.total *= a->aac.rate_mul;
					}
				}
				a->sample_rate = fmt->rate = inf->rate;
				fmt->channels = inf->channels;
				a->cache.len = 0;
			}
			a->br = ffint_mean_dyn(a->br, a->frnum, a->aac.info.bitrate);
			ffvec_addstr(&a->cache, &out);
			if (++a->frnum != DETECT_FRAMES)
				continue;
			//fall through

		case R_CACHE_DATA:
			if (t->audio.total == 0 && t->input.size)
				t->audio.total = t->input.size * 8 * a->sample_rate / a->br;
			switch (a->aac.info.aot) {
			case AAC_LC:
				t->audio.decoder = "AAC-LC"; break;
			case AAC_HE:
				t->audio.decoder = "HE-AAC"; break;
			case AAC_HEV2:
				t->audio.decoder = "HE-AACv2"; break;
			}

			t->audio.pos = apos + out.len / phi_af_size(&a->aac.fmt) - a->cache.len / phi_af_size(&a->aac.fmt);
			t->audio.pos = ffmax((ffint64)t->audio.pos, 0);
			t->data_out = *(ffstr*)&a->cache;
			a->cache.len = 0;
			a->state = R_CACHE_DONE;
			return PHI_DATA;

		case R_CACHE_DONE:
			ffvec_free(&a->cache);
			a->state = R_PASS;
			break;
		}

		if (a->state == R_PASS)
			break;
	}

	t->data_out = out;
	t->audio.pos = apos;
	return PHI_DATA;
}

const phi_filter phi_aac_dec = {
	aac_open, (void*)aac_close, (void*)aac_decode,
	"aac-decode"
};
