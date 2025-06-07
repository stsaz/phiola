/** phiola: Vorbis input
2016, Simon Zolin */

#include <avpack/vorbistag.h>
#include <avpack/base/vorbis.h>
#include <vorbis/vorbis-phi.h>

struct vorbis_dec {
	uint state;
	uint64 pos;
	vorbis_ctx *vctx;
	uint channels;
	uint pktno;
	const float **pcm; //non-interleaved
};

static void* vorbis_open(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct vorbis_dec *v = phi_track_allocT(t, struct vorbis_dec);
	t->audio.format.format = PHI_PCM_FLOAT32;
	t->audio.end_padding = (t->audio.total != ~0ULL);
	t->data_type = "pcm";
	return v;
}

static void vorbis_close(void *ctx, phi_track *t)
{
	struct vorbis_dec *v = ctx;
	if (v->vctx)
		vorbis_decode_free(v->vctx);
	phi_track_free(t, v);
}

static int vorbis_in_decode(void *ctx, phi_track *t)
{
	struct vorbis_dec *v = ctx;
	enum { R_HDR, R_TAGS, R_BOOK, R_DATA1, R_DATA };
	int r;
	ffstr in = {};
	if (t->chain_flags & PHI_FFWD)
		in = t->data_in;

	if (in.len == 0)
		goto more;

	ogg_packet opkt = {
		.packet = (void*)in.ptr,
		.bytes = in.len,
		.packetno = v->pktno++,
		.granulepos = ~0ULL,
		.e_o_s = !!(t->chain_flags & PHI_FFIRST),
	};

	switch (v->state) {
	case R_HDR: {
		uint rate, br_nominal;
		if (!vorbis_info_read(in.ptr, in.len, &v->channels, &rate, &br_nominal)) {
			errlog(t, "bad Vorbis header");
			return PHI_ERR;
		}

		if ((r = vorbis_decode_init(&v->vctx, &opkt))) {
			errlog(t, "vorbis_decode_init(): %s", vorbis_errstr(r));
			return PHI_ERR;
		}
		v->state = R_TAGS;
		return PHI_MORE;
	}

	case R_TAGS:
		if (!vorbis_tags_read(in.ptr, in.len))
			warnlog(t, "bad Vorbis tags");
		v->state = R_BOOK;
		return PHI_MORE;

	case R_BOOK:
		if ((r = vorbis_decode_init(&v->vctx, &opkt))) {
			errlog(t, "vorbis_decode_init(): %s", vorbis_errstr(r));
			return PHI_ERR;
		}
		v->state = R_DATA1;
		goto more;

	case R_DATA1:
		if (t->conf.info_only)
			return PHI_LASTOUT;

		v->state = R_DATA;
		// fallthrough

	case R_DATA:
		if (t->chain_flags & PHI_FFWD) {
			if (t->audio.pos != ~0ULL)
				v->pos = t->audio.pos;
		}
		break;
	}

	r = vorbis_decode(v->vctx, &opkt, &v->pcm);
	if (r <= 0) {
		if (r < 0)
			warnlog(t, "vorbis_decode(): %s", vorbis_errstr(r));
		goto more;
	}

	ffstr_set(&t->data_out, (void*)v->pcm, r * sizeof(float) * v->channels);
	t->audio.pos = v->pos;
	v->pos += r;

	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / pcm_size(PHI_PCM_FLOAT32, v->channels), t->audio.pos);
	return PHI_DATA;

more:
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_MORE;
}

static const phi_filter phi_vorbis_dec = {
	vorbis_open, vorbis_close, vorbis_in_decode,
	"vorbis-decode"
};
