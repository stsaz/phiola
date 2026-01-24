/** phiola: Musepack input
2017, Simon Zolin */

#include <musepack/mpc-phi.h>

struct mpc_dec {
	uint sample_size;
	uint64 pos, prev_frame_pos;
	mpc_ctx *mpc;
	uint channels;
	uint need_data :1;
	ffstr input;
	float *pcm;
};

static void* mpc_dec_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct mpc_dec *m = phi_track_allocT(t, struct mpc_dec);
	int r;
	if ((r = mpc_decode_open(&m->mpc, t->data_in.ptr, t->data_in.len))) {
		errlog(t, "mpc_decode_open(): %s", mpc_errstr(r));
		phi_track_free(t, m);
		return PHI_OPEN_ERR;
	}
	m->pcm = phi_track_alloc(t, MPC_ABUF_CAP);
	t->audio.format.format = PHI_PCM_FLOAT32;
	t->audio.format.interleaved = 1;
	m->channels = t->audio.format.channels;
	m->need_data = 1;
	m->sample_size = phi_af_size(&t->audio.format);
	t->data_in.len = 0;
	t->data_type = "pcm";
	return m;
}

static void mpc_dec_close(void *ctx, phi_track *t)
{
	struct mpc_dec *m = ctx;
	phi_track_free(t, m->pcm);
	if (m->mpc)
		mpc_decode_free(m->mpc);
	phi_track_free(t, m);
}

static int mpc_dec_process(void *ctx, phi_track *t)
{
	struct mpc_dec *m = ctx;
	int r;

	if (t->chain_flags & PHI_FFWD) {
		if (t->data_in.len != 0)
			m->input = t->data_in;
		if (m->prev_frame_pos != t->audio.pos) {
			m->pos = t->audio.pos;
			m->prev_frame_pos = t->audio.pos;
		}
	}

	if (t->audio.seek != -1) {
		if (!(t->chain_flags & PHI_FFWD)) {
			m->need_data = 1;
			return PHI_MORE;
		}
	}

	if (m->need_data) {
		if (m->input.len == 0)
			goto more;
		m->need_data = 0;
		mpc_decode_input(m->mpc, m->input.ptr, m->input.len);
		m->input.len = 0;
	}

	r = mpc_decode(m->mpc, m->pcm);
	if (r <= 0) {
		if (r < 0)
			warnlog(t, "ffmpc_decode(): %s", mpc_errstr(r));
		m->need_data = 1;
		goto more;
	}

	ffstr_set(&t->data_out, (char*)m->pcm, r * m->channels * sizeof(float));
	t->audio.pos = m->pos;
	m->pos += t->data_out.len / m->sample_size;
	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / m->sample_size, t->audio.pos);
	return PHI_DATA;

more:
	return !(t->chain_flags & PHI_FFIRST) ? PHI_MORE : PHI_DONE;
}

static const phi_filter phi_mpc_dec = {
	mpc_dec_open, mpc_dec_close, mpc_dec_process,
	"mpc-decode"
};
