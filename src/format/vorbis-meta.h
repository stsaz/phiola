/** phiola: Vorbis meta read
2022, Simon Zolin */

#include <track.h>
#include <avpack/vorbistag.h>

extern const phi_meta_if phi_metaif;

enum VORBIS_HDR_T {
	VORBIS_HDR_INFO = 1,
	VORBIS_HDR_COMMENT = 3,
};

struct vorbis_hdr {
	ffbyte type; // enum VORBIS_HDR_T
	char vorbis[6]; // ="vorbis"
};

struct vorbis_info {
	ffbyte ver[4]; // =0
	ffbyte channels;
	ffbyte rate[4];
	ffbyte br_max[4];
	ffbyte br_nominal[4];
	ffbyte br_min[4];
	ffbyte blocksize;
	ffbyte framing_bit; // =1
};

/** Read Vorbis-info packet */
static int vorbisinfo_read(ffstr pkt, ffuint *channels, ffuint *rate, ffuint *br_nominal)
{
	const struct vorbis_hdr *h = (struct vorbis_hdr*)pkt.ptr;
	if (sizeof(struct vorbis_hdr) + sizeof(struct vorbis_info) > pkt.len
		|| !(h->type == VORBIS_HDR_INFO && !ffmem_cmp(h->vorbis, "vorbis", 6)))
		return -1;

	const struct vorbis_info *vi = (struct vorbis_info*)(pkt.ptr + sizeof(struct vorbis_hdr));
	*channels = vi->channels;
	*rate = ffint_le_cpu32_ptr(vi->rate);
	*br_nominal = ffint_le_cpu32_ptr(vi->br_nominal);
	if (0 != ffint_le_cpu32_ptr(vi->ver)
		|| *channels == 0
		|| *rate == 0
		|| vi->framing_bit != 1)
		return -2;

	return 0;
}

/** Check Vorbis-comment packet.
body: Vorbis-tag data */
static int vorbiscomment_read(ffstr pkt, ffstr *body)
{
	const struct vorbis_hdr *h = (struct vorbis_hdr*)pkt.ptr;
	if (sizeof(struct vorbis_hdr) > pkt.len
		|| !(h->type == VORBIS_HDR_COMMENT && !ffmem_cmp(h->vorbis, "vorbis", 6)))
		return -1;

	*body = pkt;
	ffstr_shift(body, sizeof(struct vorbis_hdr));
	return 0;
}


struct vorbismeta {
	uint state;
	ffvec hdr;
	ffstr tags;
};

static void* vorbismeta_open(phi_track *t)
{
	struct vorbismeta *v = ffmem_new(struct vorbismeta);
	return v;
}

static void vorbismeta_close(void *ctx, phi_track *t)
{
	struct vorbismeta *v = ctx;
	ffvec_free(&v->hdr);
	ffmem_free(v);
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
		if (0 != vorbisinfo_read(t->data_in, &chan, &rate, &br)) {
			errlog(t, "vorbisinfo_read");
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
		ffstr vc;
		if (0 != vorbiscomment_read(t->data_in, &vc)) {
			errlog(t, "vorbiscomment_read");
			return PHI_ERR;
		}
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
