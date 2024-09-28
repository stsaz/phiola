/** phiola: Opus meta read
2022, Simon Zolin */

#include <track.h>
#include <avpack/vorbistag.h>

extern const phi_meta_if phi_metaif;
extern int vorbistag_read(phi_track *t, ffstr vc);

struct opus_hdr {
	char id[8]; // ="OpusHead"
	ffbyte ver;
	ffbyte channels;
	ffbyte preskip[2];
	ffbyte orig_sample_rate[4];
	//ffbyte unused[3];
};

struct opus_info {
	ffuint channels;
	ffuint rate;
	ffuint orig_rate;
	ffuint preskip;
};

/** Read OpusHead packet */
static int opusinfo_read(struct opus_info *i, ffstr pkt)
{
	const struct opus_hdr *h = (struct opus_hdr*)pkt.ptr;
	if (sizeof(struct opus_hdr) > pkt.len
		|| !!ffmem_cmp(h->id, "OpusHead", 8)
		|| h->ver != 1)
		return -1;

	i->channels = h->channels;
	i->rate = 48000;
	i->orig_rate = ffint_le_cpu32_ptr(h->orig_sample_rate);
	i->preskip = ffint_le_cpu16_ptr(h->preskip);
	return 0;
}

/** Check OpusTags packet.
body: Vorbis-tag data */
static int opuscomment_read(ffstr pkt, ffstr *body)
{
	if (8 > pkt.len
		|| ffmem_cmp(pkt.ptr, "OpusTags", 8))
		return -1;
	ffstr_set(body, pkt.ptr + 8, pkt.len - 8);
	return 0;
}


struct opusmeta {
	uint state;
	uint reset :1;
	ffvec hdr;
	ffstr tags;
};

static void* opusmeta_open(phi_track *t)
{
	struct opusmeta *o = phi_track_allocT(t, struct opusmeta);
	return o;
}

static void opusmeta_close(void *ctx, phi_track *t)
{
	struct opusmeta *o = ctx;
	ffvec_free(&o->hdr);
	phi_track_free(t, o);
}

static int opusmeta_read(void *ctx, phi_track *t)
{
	struct opusmeta *o = ctx;
	for (;;) {
		switch (o->state) {
		case 0: {
			struct opus_info info;
			if (0 != opusinfo_read(&info, t->data_in)) {
				errlog(t, "opusinfo_read");
				return PHI_ERR;
			}
			t->audio.decoder = "Opus";

			if (o->reset) {
				if (!(info.channels == t->audio.format.channels
					&& info.rate == t->audio.format.rate)) {
					errlog(t, "changing the audio format on-the-fly is not supported");
					return PHI_ERR;
				}
				t->meta_changed = 1;
			}

			struct phi_af f = {
				.format = PHI_PCM_FLOAT32,
				.channels = info.channels,
				.rate = info.rate,
			};
			t->audio.format = f;

			ffvec_addstr(&o->hdr, &t->data_in);
			o->state = 1;
			return PHI_MORE;
		}

		case 1: {
			ffstr vc;
			if (0 != opuscomment_read(t->data_in, &vc)) {
				errlog(t, "opuscomment_read");
				return PHI_ERR;
			}
			vorbistag_read(t, vc);

			o->tags = t->data_in;
			t->data_out = *(ffstr*)&o->hdr;
			o->state = 2;
			return PHI_DATA;
		}

		case 2:
			t->audio.ogg_reset = 0;
			ffvec_free(&o->hdr);
			o->state = 3;
			t->data_out = o->tags;
			return (t->conf.info_only) ? PHI_LASTOUT
				: (t->chain_flags & PHI_FFIRST) ? PHI_DONE
				: PHI_OK;

		case 3:
			if (t->audio.ogg_reset) {
				phi_metaif.destroy(&t->meta);
				o->reset = 1;
				o->state = 0;
				continue;
			}
			t->data_out = t->data_in;
			return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
		}
	}
}

const phi_filter phi_opusmeta_read = {
	opusmeta_open, opusmeta_close, opusmeta_read,
	"opus-meta-read"
};
