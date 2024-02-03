/** phiola: Musepack input
2017, Simon Zolin */

struct mpc_dec {
	ffmpc mpcdec;
	uint sample_size;
	uint64 pos, prev_frame_pos;
};

static void* mpc_dec_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct mpc_dec *m = ffmem_new(struct mpc_dec);
	if (0 != ffmpc_open(&m->mpcdec, &t->audio.format, t->data_in.ptr, t->data_in.len)) {
		errlog(t, "ffmpc_open()");
		ffmem_free(m);
		return PHI_OPEN_ERR;
	}
	m->sample_size = phi_af_size(&t->audio.format);
	t->data_in.len = 0;
	t->data_type = "pcm";
	return m;
}

static void mpc_dec_close(void *ctx, phi_track *t)
{
	struct mpc_dec *m = ctx;
	ffmpc_close(&m->mpcdec);
	ffmem_free(m);
}

static int mpc_dec_process(void *ctx, phi_track *t)
{
	struct mpc_dec *m = ctx;
	int r;

	if (t->chain_flags & PHI_FFWD) {
		if (t->data_in.len != 0) {
			ffmpc_inputblock(&m->mpcdec, t->data_in.ptr, t->data_in.len);
			t->data_in.len = 0;
		}
		if (m->prev_frame_pos != t->audio.pos) {
			m->pos = t->audio.pos;
			m->prev_frame_pos = t->audio.pos;
		}
	}

	if (t->audio.seek != -1) {
		if (!(t->chain_flags & PHI_FFWD)) {
			m->mpcdec.need_data = 1;
			return PHI_MORE;
		}
	}

	r = ffmpc_decode(&m->mpcdec, &t->data_out);

	switch (r) {
	case FFMPC_RMORE:
		if (t->chain_flags & PHI_FFIRST) {
			return PHI_DONE;
		}
		return PHI_MORE;

	case FFMPC_RDATA:
		break;

	case FFMPC_RERR:
		warnlog(t, "ffmpc_decode(): %s", ffmpc_errstr(&m->mpcdec));
		return PHI_MORE;
	}

	t->audio.pos = m->pos;
	m->pos += t->data_out.len / m->sample_size;
	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / m->sample_size, t->audio.pos);
	return PHI_DATA;
}

static const phi_filter phi_mpc_dec = {
	mpc_dec_open, mpc_dec_close, mpc_dec_process,
	"mpc-decode"
};
