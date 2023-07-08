/** phiola: MPEG-Layer3 decode
2022, Simon Zolin */

#include <mpg123/mpg123-ff.h>

typedef struct mpeg_dec {
	mpg123 *m123;
	uint64 pos;
	uint64 seek, seek_curr;
	uint fr_size;
	uint sample_rate;
} mpeg_dec;

static void mpeg_dec_close(mpeg_dec *m, phi_track *t)
{
	if (m->m123 != NULL)
		mpg123_free(m->m123);
	ffmem_free(m);
}

static void* mpeg_dec_open(phi_track *t)
{
	mpeg_dec *m = ffmem_new(mpeg_dec);

	mpg123_init();

	int err;
	if (0 != (err = mpg123_open(&m->m123, MPG123_FORCE_FLOAT))) {
		mpeg_dec_close(m, t);
		return PHI_OPEN_ERR;
	}

	m->seek = -1;
	if (t->audio.mpeg1_delay != 0)
		m->seek = t->audio.mpeg1_delay;

	t->audio.format.channels = t->audio.format.channels;
	t->audio.format.format = PHI_PCM_FLOAT32;
	t->audio.format.interleaved = 1;
	t->audio.decoder = "MPEG1-L3";
	m->fr_size = pcm_size1(&t->audio.format);
	m->sample_rate = t->audio.format.rate;
	t->data_type = "pcm";
	return m;
}

static int mpeg_dec_process(void *ctx, phi_track *t)
{
	mpeg_dec *m = ctx;
	int r = 0;
	ffstr in = {}, out = {};

	if (t->audio.seek_req) {
		// a new seek request is received, pass control to UI module
		m->seek = ~0ULL;
		m->seek_curr = ~0ULL;
		t->data_in.len = 0;
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
	}

	if (t->chain_flags & PHI_FFWD) {
		in = t->data_in;
		t->data_in.len = 0;
		m->pos = t->audio.pos;

		if (t->audio.seek != -1) {
			uint64 ns = msec_to_samples(t->audio.seek, m->sample_rate) + t->audio.mpeg1_delay;
			if (m->seek_curr != ns) {
				m->seek_curr = ns;
				mpg123_reset(m->m123);
			}
		} else if (m->seek_curr != ~0ULL) {
			m->seek_curr = ~0ULL;
		}
	}

	if (in.len != 0 || (t->chain_flags & PHI_FFIRST))
		r = mpg123_decode(m->m123, in.ptr, in.len, (ffbyte**)&out.ptr);

	if (r == 0) {
		goto end;
	} else if (r < 0) {
		errlog(t, "mpg123_decode(): %s. Near sample %U"
			, mpg123_errstr(r), t->audio.pos);
		goto end;
	}

	ffstr_set(&t->data_out, out.ptr, r);
	t->audio.pos = m->pos - t->audio.mpeg1_delay;

	uint samples = r / m->fr_size;
	if (m->seek != ~0ULL) {
		if (m->pos + samples <= m->seek) {
			m->pos += samples;
			goto end;
		}
		if (m->pos < m->seek) {
			int64 skip_samples = m->seek - m->pos;
			FF_ASSERT(skip_samples >= 0);
			t->audio.pos += skip_samples;
			ffstr_shift(&t->data_out, skip_samples * m->fr_size);
			dbglog(t, "skip %L samples", skip_samples);
		}
		m->seek = ~0ULL;
	}

	m->pos += samples;

	// skip padding
	if (t->audio.total != 0 && t->audio.mpeg1_padding != 0
		&& t->audio.pos < t->audio.total
		&& t->audio.pos + t->data_out.len / m->fr_size > t->audio.total) {

		uint n = t->audio.pos + t->data_out.len / m->fr_size - t->audio.total;
		n = ffmin(n, t->audio.mpeg1_padding);
		dbglog(t, "cut last %u samples", n);
		t->data_out.len -= n * m->fr_size;
	}

	dbglog(t, "decoded %u samples @%U"
		, samples, t->audio.pos);
	return PHI_DATA;

end:
	t->data_out.len = 0;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_MORE;
}

const phi_filter phi_mpeg_dec = {
	mpeg_dec_open, (void*)mpeg_dec_close, (void*)mpeg_dec_process,
	"mpeg-decode"
};
