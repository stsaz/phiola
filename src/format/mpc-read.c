/** phiola: .mpc reader
2017, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>
#include <avpack/mpc-read.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct mpc_r {
	mpcread mpc;
	ffstr in;
	uint64 frno;
	void *trk;
	uint sample_rate;
};

static void mpc_log(void *udata, const char *fmt, va_list va)
{
	struct mpc_r *m = udata;
	phi_dbglogv(core, NULL, m->trk, fmt, va);
}

static void* mpc_open(phi_track *t)
{
	struct mpc_r *m = phi_track_allocT(t, struct mpc_r);
	m->trk = t;
	uint64 tsize = (t->input.size != ~0ULL) ? t->input.size : 0;
	mpcread_open(&m->mpc, tsize);
	m->mpc.log = mpc_log;
	m->mpc.udata = m;
	return m;
}

static void mpc_close(void *ctx, phi_track *t)
{
	struct mpc_r *m = ctx;
	mpcread_close(&m->mpc);
	phi_track_free(t, m);
}

#define brate(bytes, samples, rate) \
	FFINT_DIVSAFE((uint64)(bytes) * 8 * (rate), samples)

static void mpc_info(struct mpc_r *m, phi_track *t, const struct mpcread_info *info)
{
	t->audio.format.rate = info->sample_rate;
	t->audio.format.channels = info->channels;

	if (t->input.size != ~0ULL)
		t->audio.bitrate = brate(t->input.size, info->total_samples, info->sample_rate);

	t->audio.total = info->total_samples;
	t->audio.decoder = "Musepack";
}

extern const phi_meta_if phi_metaif;
static int mpc_process(void *ctx, phi_track *t)
{
	struct mpc_r *m = ctx;
	int r;
	ffstr blk;

	if (t->chain_flags & PHI_FSTOP) {

		return PHI_LASTOUT;
	}

	if (t->data_in.len != 0) {
		m->in = t->data_in;
		t->data_in.len = 0;
	}

	if (t->audio.seek_req && t->audio.seek != -1 && m->sample_rate != 0) {
		t->audio.seek_req = 0;
		mpcread_seek(&m->mpc, msec_to_samples(t->audio.seek, m->sample_rate));
		dbglog(t, "seek: %Ums", t->audio.seek);
	}

	for (;;) {
		r = mpcread_process(&m->mpc, &m->in, &blk);

		switch (r) {

		case MPCREAD_HEADER:
			mpc_info(m, t, mpcread_info(&m->mpc));

			if (!core->track->filter(t, core->mod("mpc.decode"), 0))
				return PHI_ERR;

			m->sample_rate = t->audio.format.rate;
			t->data_out = blk;
			return PHI_DATA;

		case MPCREAD_MORE:
			return PHI_MORE;

		case MPCREAD_SEEK:
			t->input.seek = mpcread_offset(&m->mpc);
			return PHI_MORE;

		case MPCREAD_TAG: {
			ffstr name, val;
			int r = mpcread_tag(&m->mpc, &name, &val);
			if (r != 0)
				ffstr_setz(&name, ffmmtag_str[r]);
			dbglog(t, "tag: %S: %S", &name, &val);
			phi_metaif.set(&t->meta, name, val, 0);
			continue;
		}

		case MPCREAD_DATA:
			goto data;

		case MPCREAD_DONE:

			return PHI_LASTOUT;

		case MPCREAD_WARN:
			warnlog(t, "mpcread_process(): %s.  Offset: %U"
				, mpcread_error(&m->mpc), mpcread_offset(&m->mpc));
			continue;
		case MPCREAD_ERROR:
			errlog(t, "mpcread_process(): %s.  Offset: %U"
				, mpcread_error(&m->mpc), mpcread_offset(&m->mpc));
			return PHI_ERR;
		}
	}

data:
	t->audio.pos = mpcread_cursample(&m->mpc);
	dbglog(t, "frame#%U passing %L bytes @%U"
		, ++m->frno, blk.len, t->audio.pos);
	t->data_out = blk;
	return PHI_DATA;
}

const phi_filter phi_mpc_read = {
	mpc_open, (void*)mpc_close, (void*)mpc_process,
	"mpc-read"
};
