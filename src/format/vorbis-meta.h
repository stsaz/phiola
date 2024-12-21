/** phiola: Vorbis meta read
2022, Simon Zolin */

#include <track.h>
#include <avpack/vorbistag.h>
#include <avpack/vorbis-fmt.h>

extern const phi_meta_if phi_metaif;

struct vorbismeta {
	uint state;
	ffvec hdr;
	ffstr tags;
};

static void* vorbismeta_open(phi_track *t)
{
	struct vorbismeta *v = phi_track_allocT(t, struct vorbismeta);
	return v;
}

static void vorbismeta_close(void *ctx, phi_track *t)
{
	struct vorbismeta *v = ctx;
	ffvec_free(&v->hdr);
	phi_track_free(t, v);
}

int vorbistag_read(phi_track *t, ffstr vc)
{
	vorbistagread vtag = {};
	for (;;) {
		ffstr name, val;
		int tag = vorbistagread_process(&vtag, &vc, &name, &val);
		switch (tag) {
		case VORBISTAGREAD_DONE:
			return 0;
		case VORBISTAGREAD_ERROR:
			errlog(t, "vorbistagread_process");
			return -1;
		}

		dbglog(t, "%S: %S", &name, &val);
		if (tag != 0)
			ffstr_setz(&name, ffmmtag_str[tag]);
		phi_metaif.set(&t->meta, name, val, 0);
	}
}

static int vorbismeta_read(void *ctx, phi_track *t)
{
	struct vorbismeta *v = ctx;
	switch (v->state) {
	case 0: {
		uint chan, rate, br;
		if (!vorbis_info_read(t->data_in.ptr, t->data_in.len, &chan, &rate, &br)) {
			errlog(t, "vorbis_info_read");
			return PHI_ERR;
		}
		t->audio.decoder = "Vorbis";
		struct phi_af f = {
			.format = PHI_PCM_FLOAT32,
			.channels = chan,
			.rate = rate,
		};
		t->audio.format = f;
		t->audio.bitrate = br;

		ffvec_addstr(&v->hdr, &t->data_in);
		v->state = 1;
		return PHI_MORE;
	}

	case 1: {
		int r;
		if (!(r = vorbis_tags_read(t->data_in.ptr, t->data_in.len))) {
			errlog(t, "vorbiscomment_read");
			return PHI_ERR;
		}
		ffstr vc = t->data_in;
		ffstr_shift(&vc, r);
		vorbistag_read(t, vc);

		v->tags = t->data_in;
		t->data_out = *(ffstr*)&v->hdr;
		v->state = 2;
		return PHI_DATA;
	}
	}

	t->data_out = v->tags;
	return (t->conf.info_only) ? PHI_LASTOUT : PHI_DONE;
}

const phi_filter phi_vorbismeta_read = {
	vorbismeta_open, vorbismeta_close, vorbismeta_read,
	"vorbis-meta-read"
};
