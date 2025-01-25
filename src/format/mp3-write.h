/** phiola: .mp3 write
2017,2021, Simon Zolin */

#include <avpack/mp3-write.h>

typedef struct mp3_w {
	uint state;
	uint nframe;
	uint flags;
	ffstr in;
	mp3write mpgw;
} mp3_w;

void* mpeg_out_open(phi_track *t)
{
	mp3_w *m = phi_track_allocT(t, mp3_w);
	mp3write_create(&m->mpgw);
	if (t->conf.stream_copy) {
		m->mpgw.options |= MP3WRITE_XINGTAG;
		m->mpgw.vbr_scale = (int)t->audio.mpeg1_vbr_scale - 1;
	}
	m->mpgw.id3v2_min_size = 1000;
	return m;
}

void mpeg_out_close(void *ctx, phi_track *t)
{
	mp3_w *m = ctx;
	mp3write_close(&m->mpgw);
	phi_track_free(t, m);
}

int mpeg_out_addmeta(mp3_w *m, phi_track *t)
{
	uint i = 0;
	ffstr name, val;
	while (core->metaif->list(&t->meta, &i, &name, &val, PHI_META_UNIQUE)) {
		if (ffstr_eqcz(&name, "vendor"))
			continue;
		int tag;
		if (-1 == (tag = ffszarr_find(ffmmtag_str, FF_COUNT(ffmmtag_str), name.ptr, name.len))
			|| tag == MMTAG_VENDOR)
			continue;

		if (0 != mp3write_addtag(&m->mpgw, tag, val)) {
			warnlog(t, "can't add tag: %S", &name);
		}
	}
	return 0;
}

int mpeg_out_process(void *ctx, phi_track *t)
{
	mp3_w *m = ctx;
	int r;

	switch (m->state) {
	case 0:
		if (0 != mpeg_out_addmeta(m, t))
			return PHI_ERR;
		m->state = 2;
		if (ffsz_eq(t->data_type, "mpeg"))
			break;
		else if (!ffsz_eq(t->data_type, "pcm")) {
			errlog(t, "unsupported input data format: %s", t->data_type);
			return PHI_ERR;
		}

		if (!core->track->filter(t, core->mod("ac-mpeg.encode"), PHI_TF_PREV)) {
			t->error = PHI_E_OUT_FMT;
			return PHI_ERR;
		}
		return PHI_MORE;

	case 2:
		break;
	}

	if (t->chain_flags & PHI_FFIRST)
		m->flags |= MP3WRITE_FLAST;

	ffstr out;
	if (t->data_in.len != 0) {
		m->in = t->data_in;
		t->data_in.len = 0;
	}

	for (;;) {
		r = mp3write_process(&m->mpgw, &m->in, &out, m->flags);
		switch (r) {

		case MP3WRITE_DATA:
			dbglog(t, "frame #%u: %L bytes"
				, m->nframe++, out.len);
			goto data;

		case MP3WRITE_MORE:
			return PHI_MORE;

		case MP3WRITE_SEEK:
			t->output.seek = mp3write_offset(&m->mpgw);
			continue;

		case MP3WRITE_DONE:

			return PHI_DONE;

		default:
			errlog(t, "mp3write_process() failed");
			return PHI_ERR;
		}
	}

data:
	t->data_out = out;
	return PHI_DATA;
}

const phi_filter phi_mp3_write = {
	mpeg_out_open, (void*)mpeg_out_close, (void*)mpeg_out_process,
	"mp3-write"
};
