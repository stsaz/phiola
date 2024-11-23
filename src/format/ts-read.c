/** phiola: .ts reader
2024, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>
#include <avpack/ts-read.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct ts_r {
	tsread ts;
	ffstr in;
	uint track_id;
	uint64 n_pkt;
	uint64 pos_start_msec;
	void *trk;
};

static void ts_log(void *udata, const char *fmt, va_list va)
{
	struct ts_r *c = udata;
	phi_dbglogv(core, NULL, c->trk, fmt, va);
}

static void* ts_open(phi_track *t)
{
	struct ts_r *c = phi_track_allocT(t, struct ts_r);
	c->trk = t;
	c->pos_start_msec = ~0ULL;
	uint64 tsize = (t->input.size != ~0ULL) ? t->input.size : 0;
	tsread_open(&c->ts, tsize);
	t->input.size = ~0ULL; // prevent file seek requests from the format filter
	c->ts.log = ts_log;
	c->ts.udata = c;
	return c;
}

static void ts_close(void *ctx, phi_track *t)
{
	struct ts_r *c = ctx;
	tsread_close(&c->ts);
	phi_track_free(t, c);
}

static int ts_info(struct ts_r *c, phi_track *t, const struct _tsr_pm *info)
{
	const char *mod;
	switch (info->stream_type) {
	case TS_STREAM_AUDIO_MP3:
		mod = "format.mp3";  break;
	case TS_STREAM_AUDIO_AAC:
		mod = "format.aac";  break;
	default:
		dbglog(t, "codec not supported: %u", info->stream_type);
		return 1;
	}
	if (!core->track->filter(t, core->mod(mod), 0))
		return -1;

	c->track_id = info->pid;
	return 0;
}

static int ts_process(void *ctx, phi_track *t)
{
	struct ts_r *c = ctx;
	int r;
	ffstr pkt;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->data_in.len) {
		c->in = t->data_in;
		t->data_in.len = 0;
	}

	for (;;) {
		r = tsread_process(&c->ts, &c->in, &pkt);

		switch ((enum TSREAD_R)r) {

		case TSREAD_DATA:
			if (c->n_pkt == 0) {
				// select the first audio track with a recognized codec
				r = ts_info(c, t, tsread_info(&c->ts));
				if (r < 0)
					return PHI_ERR;
				else if (r > 0)
					continue;
			}

			if (c->track_id != tsread_info(&c->ts)->pid)
				continue;

			goto data;

		case TSREAD_MORE:
			return PHI_MORE;

		// case TSREAD_DONE:
		// 	return PHI_LASTOUT;

		case TSREAD_WARN:
			warnlog(t, "tsread_process(): %s.  Offset: %U"
				, tsread_error(&c->ts), tsread_offset(&c->ts));
			continue;

		case TSREAD_ERROR:
			errlog(t, "tsread_process(): %s.  Offset: %U"
				, tsread_error(&c->ts), tsread_offset(&c->ts));
			return PHI_ERR;
		}
	}

data:
	if (c->pos_start_msec == ~0ULL)
		c->pos_start_msec = tsread_pos_msec(&c->ts);
	if (t->audio.format.rate)
		t->audio.pos = msec_to_samples(tsread_pos_msec(&c->ts) - c->pos_start_msec, t->audio.format.rate);
	dbglog(t, "frame#%U passing %L bytes @%U"
		, c->n_pkt, pkt.len, t->audio.pos);
	c->n_pkt++;
	t->data_out = pkt;
	return PHI_DATA;
}

const phi_filter phi_ts_read = {
	ts_open, (void*)ts_close, (void*)ts_process,
	"ts-read"
};
