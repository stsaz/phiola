/** phiola: AAC decode
2016, Simon Zolin */

#include <acodec/alib3-bridge/aac.h>

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
	struct aac_in *a = ffmem_new(struct aac_in);
	a->aac.enc_delay = t->audio.start_delay;
	a->aac.end_padding = t->audio.end_padding;
	a->aac.total_samples = t->audio.total;
	a->aac.contr_samprate = t->audio.format.rate;
	if (0 != ffaac_open(&a->aac, t->audio.format.channels, t->data_in.ptr, t->data_in.len)) {
		errlog(t, "ffaac_open(): %s", ffaac_errstr(&a->aac));
		ffmem_free(a);
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
	ffmem_free(a);
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
	uint fr_len = 0;
	enum { R_CACHE, R_CACHE_DATA, R_CACHE_DONE, R_PASS };

	if (t->chain_flags & PHI_FFWD) {
		ffaac_input(&a->aac, t->data_in.ptr, t->data_in.len, t->audio.pos);
		t->data_in.len = 0;
		if (t->audio.seek != -1) {
			uint64 seek = msec_to_samples(t->audio.seek, a->sample_rate);
			ffaac_seek(&a->aac, seek);
		}
	}

	for (;;) {

	r = ffaac_decode(&a->aac);
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
		dbglog(t, "decoded %u samples (%U)"
			, a->aac.pcmlen / pcm_size1(&a->aac.fmt), ffaac_cursample(&a->aac));
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
					a->aac.total_samples = t->audio.total;
				}
			}
			a->sample_rate = fmt->rate = inf->rate;
			fmt->channels = inf->channels;
			a->cache.len = 0;
		}
		a->br = ffint_mean_dyn(a->br, a->frnum, a->aac.info.bitrate);
		ffvec_add(&a->cache, a->aac.pcm, a->aac.pcmlen, 1);
		if (++a->frnum != DETECT_FRAMES)
			continue;
		fr_len = a->aac.pcmlen;
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

		t->audio.pos = ffaac_cursample(&a->aac) + fr_len / pcm_size1(&a->aac.fmt) - a->cache.len / pcm_size1(&a->aac.fmt);
		t->audio.pos = ffmax((ffint64)t->audio.pos, 0);
		ffstr_setstr(&t->data_out, &a->cache);
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

	t->audio.pos = ffaac_cursample(&a->aac);
	ffstr_set(&t->data_out, (void*)a->aac.pcm, a->aac.pcmlen);
	return PHI_DATA;
}

const phi_filter phi_aac_dec = {
	aac_open, (void*)aac_close, (void*)aac_decode,
	"aac-decode"
};
