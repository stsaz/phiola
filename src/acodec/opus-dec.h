/** phiola: Opus input
2016, Simon Zolin */

#include <acodec/alib3-bridge/opus-dec-if.h>

struct opus_dec {
	uint state;
	ffopus opus;
	uint sample_size;
	uint64 prev_page_pos;
	uint reset_decoder;
};

static void* opus_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct opus_dec *o = phi_track_allocT(t, struct opus_dec);

	if (ffopus_open(&o->opus)) {
		errlog(t, "ffopus_open(): %s", ffopus_errstr(&o->opus));
		phi_track_free(t, o);
		return PHI_OPEN_ERR;
	}

	o->prev_page_pos = ~0ULL;
	return o;
}

static void opus_close(void *ctx, phi_track *t)
{
	struct opus_dec *o = ctx;
	ffopus_close(&o->opus);
	phi_track_free(t, o);
}

static const char* opus_pkt_mode(const char *d)
{
	if (d[0] & 0x80)
		return "celt";
	else if ((d[0] & 0x60) == 0x60)
		return "hybrid";
	return "silk";
}

static int opus_in_decode(void *ctx, phi_track *t)
{
	enum { R_HDR, R_TAGS, R_DATA1, R_DATA };
	struct opus_dec *o = ctx;
	int r;
	const char *opus_mode = NULL;

	ffstr in = {};
	if (t->chain_flags & PHI_FFWD) {
		in = t->data_in;
		t->data_in.len = 0;

		if (core->conf.log_level >= PHI_LOG_DEBUG && in.len)
			opus_mode = opus_pkt_mode(in.ptr);
	}

	ffstr out;
	for (;;) {
		switch (o->state) {
		case R_HDR:
		case R_TAGS:
			if (!(t->chain_flags & PHI_FFWD))
				return PHI_MORE;

			o->state++;
			break;

		case R_DATA1:
			if (t->audio.total != ~0ULL) {
				t->audio.total -= o->opus.info.preskip;
				t->audio.end_padding = 1;
			}

			if (t->conf.info_only)
				return PHI_LASTOUT;

			o->state = R_DATA;
			// fallthrough

		case R_DATA:
			if (t->audio.seek_req) {
				// a new seek request is received, pass control to UI module
				o->reset_decoder = 1;
				t->data_in.len = 0;
				return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
			}

			if (t->chain_flags & PHI_FFWD) {
				if (o->reset_decoder) {
					o->reset_decoder = 0;
					opus_decode_reset(o->opus.dec);
				}

				if (o->opus.pos == ~0ULL || o->prev_page_pos != t->audio.pos) {
					o->prev_page_pos = t->audio.pos;
					ffopus_setpos(&o->opus, t->audio.pos);
				}
			}
			break;
		}

		for (;;) {
			r = ffopus_decode(&o->opus, &in, &out, &t->audio.pos);

			switch (r) {
			case FFOPUS_RHDR:
				t->audio.format.format = PHI_PCM_FLOAT32;
				t->audio.format.interleaved = 1;
				o->sample_size = phi_af_size(&t->audio.format);
				t->audio.start_delay = o->opus.info.preskip;
				t->data_type = "pcm";
				break;

			case FFOPUS_RHDRFIN:
				goto again; // this packet isn't a Tags packet but audio data
			case FFOPUS_RHDRFIN_TAGS:
				break;

			case FFOPUS_RDATA:
				goto data;

			case FFOPUS_RERR:
				errlog(t, "ffopus_decode(): %s", ffopus_errstr(&o->opus));
				return PHI_ERR;

			case FFOPUS_RWARN:
				warnlog(t, "ffopus_decode(): %s", ffopus_errstr(&o->opus));
				// fallthrough

			case FFOPUS_RMORE:
				if (t->chain_flags & PHI_FFIRST) {
					return PHI_DONE;
				}
				return PHI_MORE;
			}
		}

again:
		;
	}

data:
	dbglog(t, "decoded %L samples @%U  mode:%s"
		, out.len / o->sample_size, t->audio.pos, opus_mode);
	t->data_out = out;
	return PHI_DATA;
}

static const phi_filter phi_opus_dec = {
	opus_open, opus_close, opus_in_decode,
	"opus-decode"
};
