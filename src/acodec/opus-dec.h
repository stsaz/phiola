/** phiola: Opus input
2016, Simon Zolin */

struct opus_dec {
	uint state;
	ffopus opus;
	uint sampsize;
	uint64 pagepos;
	uint last :1;
};

static void* opus_open(phi_track *t)
{
	struct opus_dec *o = ffmem_new(struct opus_dec);

	if (0 != ffopus_open(&o->opus)) {
		errlog(t, "ffopus_open(): %s", ffopus_errstr(&o->opus));
		ffmem_free(o);
		return PHI_OPEN_ERR;
	}

	o->pagepos = (uint64)-1;
	return o;
}

static void opus_close(void *ctx, phi_track *t)
{
	struct opus_dec *o = ctx;
	ffopus_close(&o->opus);
	ffmem_free(o);
}

static int opus_in_decode(void *ctx, phi_track *t)
{
	enum { R_HDR, R_TAGS, R_DATA1, R_DATA };
	struct opus_dec *o = ctx;
	int reset = 0;
	int r;

	ffstr in = {};
	if (t->chain_flags & PHI_FFWD) {
		in = t->data_in;
		t->data_in.len = 0;
	}

again:
	switch (o->state) {
	case R_HDR:
	case R_TAGS:
		if (!(t->chain_flags & PHI_FFWD))
			return PHI_MORE;

		o->state++;
		break;

	case R_DATA1:
		if (t->audio.total != ~0ULL) {
			o->opus.total_samples = t->audio.total;
			t->audio.total -= o->opus.info.preskip;
		}

		if (t->conf.info_only)
			return PHI_LASTOUT;

		o->state = R_DATA;
		// fallthrough

	case R_DATA:
		if ((t->chain_flags & PHI_FFWD) && t->audio.seek != -1) {
			uint64 seek = msec_to_samples(t->audio.seek, o->opus.info.rate);
			ffopus_seek(&o->opus, seek);
			reset = 1;
		}
		if (t->chain_flags & PHI_FFWD) {
			if (o->pagepos != t->audio.pos) {
				ffopus_setpos(&o->opus, t->audio.pos, reset);
				o->pagepos = t->audio.pos;
			}
		}
		break;
	}

	if (o->last) {
		return PHI_DONE;
	}

	ffstr out;
	for (;;) {

		r = ffopus_decode(&o->opus, &in, &out);

		switch (r) {

		case FFOPUS_RHDR:
			t->audio.format.format = PHI_PCM_FLOAT32;
			t->audio.format.interleaved = 1;
			o->sampsize = pcm_size1(&t->audio.format);
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

data:
	t->audio.pos = ffopus_startpos(&o->opus);
	dbglog(t, "decoded %L samples (at %U)"
		, out.len / o->sampsize, t->audio.pos);
	t->data_out = out;
	return PHI_DATA;
}

static const phi_filter phi_opus_dec = {
	opus_open, opus_close, opus_in_decode,
	"opus-decode"
};
