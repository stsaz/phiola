/** phiola: skip audio samples
2024, Simon Zolin */

#include <track.h>

struct pcm_skip {
	uint sample_size;
	uint ignore_delay;
};

static int audio_skip(struct pcm_skip *c, phi_track *t, ffstr *buf, uint64 skip)
{
	uint64 bytes = skip * c->sample_size;
	buf->len -= bytes;
	if ((ssize_t)buf->len < 0)
		return -1;

	if (t->audio.format.interleaved) {
		buf->ptr += bytes;

	} else {
		bytes /= t->audio.format.channels;
		const u_char **chan = (const u_char**)buf->ptr;
		for (uint i = 0;  i != t->audio.format.channels;  i++) {
			chan[i] = chan[i] + bytes;
		}
	}
	return 0;
}

static void* pcm_skip_open(phi_track *t)
{
	struct pcm_skip *c = ffmem_new(struct pcm_skip);
	return c;
}

static void pcm_skip_close(void *ctx, phi_track *t)
{
	struct pcm_skip *c = ctx;
	ffmem_free(c);
}

static int pcm_skip_process(void *ctx, phi_track *t)
{
	struct pcm_skip *c = ctx;
	t->data_out = t->data_in;

	if (t->chain_flags & PHI_FFIRST) {
		return PHI_DONE;
	}

	if (t->audio.pos == ~0ULL)
		return PHI_OK;

	/*
	[delay=500, padding=1000, total=96000]
	Input:  0 500 48000 48500 96500 97500
	Real:     [ 0       48000 96000 ]

	real_pos = input_pos - delay
	seek_skip = seek_pos - real_pos
	*/

	if (c->sample_size == 0) {
		c->sample_size = phi_af_size(&t->audio.format);
		if (t->audio.start_delay
			&& t->audio.start_delay < t->audio.pos + t->data_out.len / c->sample_size) {
			audio_skip(c, t, &t->data_out, t->audio.start_delay);
			dbglog(t, "@%U  skipped:%u"
				, t->audio.pos, (int)t->audio.start_delay);
			t->audio.pos += t->audio.start_delay;
		} else {
			c->ignore_delay = 1;
		}
	}

	if (!c->ignore_delay)
		t->audio.pos -= t->audio.start_delay;

	if (!t->audio.seek_req && t->audio.seek != -1) {
		uint64 seek_samples = pcm_samples(t->audio.seek, t->audio.format.rate);
		uint64 skip = seek_samples - t->audio.pos;
		if ((int64)skip > 0) {
			if (audio_skip(c, t, &t->data_out, skip))
				return PHI_MORE;
			t->audio.pos += skip;
			dbglog(t, "@%U  skipped:%U", t->audio.pos, skip);
		}
	}

	uint64 samples = t->data_out.len / c->sample_size;
	if (t->audio.end_padding
		&& t->audio.total != ~0ULL
		&& t->audio.pos <= t->audio.total
		&& t->audio.pos + samples > t->audio.total) {
		dbglog(t, "@%U  cutoff:%U", t->audio.pos, samples - (t->audio.total - t->audio.pos));
		samples = t->audio.total - t->audio.pos;
		t->data_out.len = samples * c->sample_size;
	}

	return PHI_OK;
}

const phi_filter phi_pcm_skip = {
	pcm_skip_open, pcm_skip_close, pcm_skip_process,
	"pcm-skip"
};
