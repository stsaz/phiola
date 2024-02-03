/** Musepack decoder interface
2017, Simon Zolin */

#pragma once
#include <afilter/pcm.h>
#include <ffbase/string.h>
#include <musepack/mpc-phi.h>

typedef struct ffmpc {
	mpc_ctx *mpc;
	int err;
	uint channels;
	uint need_data :1;

	ffstr input;

	float *pcm;
} ffmpc;

static inline void ffmpc_inputblock(ffmpc *m, const char *block, size_t len)
{
	ffstr_set(&m->input, block, len);
}

enum {
	FFMPC_RMORE,
	FFMPC_RDATA,
	FFMPC_RERR,
};

#define ERR(m, r) \
	(m)->err = (r),  FFMPC_RERR

enum {
	FFMPC_ESYS = 1,
};

const char* ffmpc_errstr(ffmpc *m)
{
	if (m->err < 0)
		return mpc_errstr(m->err);
	switch (m->err) {
	case FFMPC_ESYS:
		return fferr_strptr(fferr_last());
	}
	return "";
}

int ffmpc_open(ffmpc *m, struct phi_af *fmt, const char *conf, size_t len)
{
	if (0 != (m->err = mpc_decode_open(&m->mpc, conf, len)))
		return -1;
	if (NULL == (m->pcm = ffmem_alloc(MPC_ABUF_CAP)))
		return m->err = FFMPC_ESYS,  -1;
	fmt->format = PHI_PCM_FLOAT32;
	fmt->interleaved = 1;
	m->channels = fmt->channels;
	m->need_data = 1;
	return 0;
}

void ffmpc_close(ffmpc *m)
{
	ffmem_free(m->pcm);
	mpc_decode_free(m->mpc);
}

/** Decode 1 frame. */
int ffmpc_decode(ffmpc *m, ffstr *out)
{
	if (m->need_data) {
		if (m->input.len == 0)
			return FFMPC_RMORE;
		m->need_data = 0;
		mpc_decode_input(m->mpc, m->input.ptr, m->input.len);
		m->input.len = 0;
	}

	int r = mpc_decode(m->mpc, m->pcm);
	if (r == 0) {
		m->need_data = 1;
		return FFMPC_RMORE;
	} else if (r < 0) {
		m->need_data = 1;
		return ERR(m, r);
	}

	ffstr_set(out, (char*)m->pcm, r * m->channels * sizeof(float));
	return FFMPC_RDATA;
}
