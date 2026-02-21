/** phiola: MPEG-Layer3 decode
2022, Simon Zolin */

#include <mpg123/mpg123-phi.h>

struct mpeg_dec {
	phi_mpg123 *m123;
	uint64 prev_frame_pos;
	uint fr_size;
	uint reset_decoder;
};

static void mpeg_dec_close(struct mpeg_dec *m, phi_track *t)
{
	if (m->m123 != NULL)
		phi_mpg123_free(m->m123);
	phi_track_free(t, m);
}

static void* mpeg_dec_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct mpeg_dec *m = phi_track_allocT(t, struct mpeg_dec);

	phi_mpg123_init();

	int err;
	if (0 != (err = phi_mpg123_open(&m->m123, 0))) {
		mpeg_dec_close(m, t);
		return PHI_OPEN_ERR;
	}

	t->audio.format.format = PHI_PCM_FLOAT32;
	t->audio.format.interleaved = 1;
	t->audio.decoder = "MP3";
	m->fr_size = phi_af_size(&t->audio.format);
	t->data_type = PHI_AC_PCM;
	return m;
}

static int mpeg_dec_process(void *ctx, phi_track *t)
{
	struct mpeg_dec *m = ctx;
	int r = 0;
	uint64 prev_pos = m->prev_frame_pos;
	ffstr in = {}, out = {};

	if (t->audio.seek_req) {
		// a new seek request is received, pass control to UI module
		m->reset_decoder = 1;
		t->data_in.len = 0;
		return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
	}

	if (t->chain_flags & PHI_FFWD) {
		in = t->data_in;
		t->data_in.len = 0;
		m->prev_frame_pos = t->audio.pos;
		if (m->reset_decoder) {
			m->reset_decoder = 0;
			phi_mpg123_reset(m->m123);
		}
	}

	if (in.len != 0 || (t->chain_flags & PHI_FFIRST))
		r = phi_mpg123_decode(m->m123, in.ptr, in.len, (ffbyte**)&out.ptr);

	if (r == 0) {
		goto end;
	} else if (r < 0) {
		errlog(t, "phi_mpg123_decode(): %s. Near sample %U"
			, phi_mpg123_error(r), t->audio.pos);
		goto end;
	}

	ffstr_set(&t->data_out, out.ptr, r);
	t->audio.pos = prev_pos;

	dbglog(t, "decoded %u samples @%U"
		, r / m->fr_size, t->audio.pos);
	return PHI_DATA;

end:
	t->data_out.len = 0;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_MORE;
}

const phi_filter phi_mpeg_dec = {
	mpeg_dec_open, (void*)mpeg_dec_close, (void*)mpeg_dec_process,
	"mpeg-decode"
};
