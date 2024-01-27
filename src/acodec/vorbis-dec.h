/** phiola: Vorbis input
2016, Simon Zolin */

struct vorbis_dec {
	uint state;
	uint64 pagepos;
	ffvorbis vorbis;
};

static void* vorbis_open(phi_track *t)
{
	struct vorbis_dec *v = ffmem_new(struct vorbis_dec);

	if (0 != ffvorbis_open(&v->vorbis)) {
		errlog(t, "ffvorbis_open(): %s", ffvorbis_errstr(&v->vorbis));
		ffmem_free(v);
		return PHI_OPEN_ERR;
	}

	return v;
}

static void vorbis_close(void *ctx, phi_track *t)
{
	struct vorbis_dec *v = ctx;
	ffvorbis_close(&v->vorbis);
	ffmem_free(v);
}

/*
Stream copy:
Pass the first 2 packets with meta_block flag, then close the filter.
*/
static int vorbis_in_decode(void *ctx, phi_track *t)
{
	enum { R_HDR, R_TAGS, R_BOOK, R_DATA1, R_DATA };
	struct vorbis_dec *v = ctx;

	switch (v->state) {
	case R_HDR:
	case R_TAGS:
	case R_BOOK:
		if (!(t->chain_flags & PHI_FFWD))
			return PHI_MORE;

		v->state++;
		break;

	case R_DATA1:
		if (t->conf.info_only)
			return PHI_LASTOUT;

		if (t->audio.total != ~0ULL && t->audio.total != 0)
			v->vorbis.total_samples = t->audio.total;
		v->state = R_DATA;
		// fallthrough

	case R_DATA:
		if ((t->chain_flags & PHI_FFWD) && t->audio.seek != -1) {
			uint64 seek = msec_to_samples(t->audio.seek, ffvorbis_rate(&v->vorbis));
			ffvorbis_seek(&v->vorbis, seek);
		}
		break;
	}

	int r;
	ffstr in = {};
	if (t->chain_flags & PHI_FFWD) {
		in = t->data_in;
		t->data_in.len = 0;
		v->vorbis.fin = !!(t->chain_flags & PHI_FFIRST);

		if (v->pagepos != t->audio.pos) {
			v->vorbis.cursample = t->audio.pos;
			v->pagepos = t->audio.pos;
		}
	}

	for (;;) {

		r = ffvorbis_decode(&v->vorbis, in.ptr, in.len, &t->data_out);

		switch (r) {

		case FFVORBIS_RHDR:
			t->audio.format.interleaved = 0;
			t->data_type = "pcm";
			return PHI_MORE;

		case FFVORBIS_RHDRFIN:
			return PHI_MORE;

		case FFVORBIS_RDATA:
			goto data;

		case FFVORBIS_RERR:
			errlog(t, "ffvorbis_decode(): %s", ffvorbis_errstr(&v->vorbis));
			return PHI_ERR;

		case FFVORBIS_RWARN:
			warnlog(t, "ffvorbis_decode(): %s", ffvorbis_errstr(&v->vorbis));
			// fallthrough

		case FFVORBIS_RMORE:
			if (t->chain_flags & PHI_FFIRST) {
				return PHI_DONE;
			}
			return PHI_MORE;
		}
	}

data:
	t->audio.pos = ffvorbis_cursample(&v->vorbis);
	dbglog(t, "decoded %L samples (at %U)"
		, t->data_out.len / pcm_size(PHI_PCM_FLOAT32, ffvorbis_channels(&v->vorbis)), t->audio.pos);
	return PHI_DATA;
}

static const phi_filter phi_vorbis_dec = {
	vorbis_open, vorbis_close, vorbis_in_decode,
	"vorbis-decode"
};
