/** phiola: Musepack input
2017, Simon Zolin */

struct mpc_dec {
	ffmpc mpcdec;
	struct phi_af fmt;
};

static void* mpc_dec_open(phi_track *t)
{
	struct mpc_dec *m = ffmem_new(struct mpc_dec);
	if (0 != ffmpc_open(&m->mpcdec, &t->audio.format, t->data_in.ptr, t->data_in.len)) {
		errlog(t, "ffmpc_open()");
		ffmem_free(m);
		return PHI_OPEN_ERR;
	}
	m->fmt = t->audio.format;
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
	ffstr s;

	if ((t->chain_flags & PHI_FFWD) && t->data_in.len != 0) {
		ffmpc_inputblock(&m->mpcdec, t->data_in.ptr, t->data_in.len, t->audio.pos);
		t->data_in.len = 0;
	}

	if (t->audio.seek != -1) {
		if (t->chain_flags & PHI_FFWD) {
			uint64 seek = msec_to_samples(t->audio.seek, m->fmt.rate);
			ffmpc_seek(&m->mpcdec, seek);
		} else {
			m->mpcdec.need_data = 1;
			return PHI_MORE;
		}
	}

	r = ffmpc_decode(&m->mpcdec);

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

	ffmpc_audiodata(&m->mpcdec, &s);
	dbglog(t, "decoded %L samples"
		, s.len / pcm_size1(&m->fmt));
	ffstr_setstr(&t->data_out, &s);
	t->audio.pos = ffmpc_cursample(&m->mpcdec);
	return PHI_DATA;
}

static const phi_filter phi_mpc_dec = {
	mpc_dec_open, mpc_dec_close, mpc_dec_process,
	"mpc-decode"
};
