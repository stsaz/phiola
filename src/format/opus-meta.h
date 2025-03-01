/** phiola: Opus meta read
2022, Simon Zolin */

#include <track.h>
#include <avpack/vorbistag.h>
#include <avpack/base/opus.h>

extern int vorbistag_read(phi_track *t, ffstr vc);

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
			uint channels, preskip;
			if (!opus_hdr_read(t->data_in.ptr, t->data_in.len, &channels, &preskip)) {
				errlog(t, "opus_hdr_read");
				return PHI_ERR;
			}
			t->audio.decoder = "Opus";

			if (o->reset) {
				if (!(channels == t->audio.format.channels
					&& 48000 == t->audio.format.rate)) {
					errlog(t, "changing the audio format on-the-fly is not supported");
					return PHI_ERR;
				}
				t->meta_changed = 1;
			}

			struct phi_af f = {
				.format = PHI_PCM_FLOAT32,
				.channels = channels,
				.rate = 48000,
			};
			t->audio.format = f;

			ffvec_addstr(&o->hdr, &t->data_in);
			o->state = 1;
			return PHI_MORE;
		}

		case 1: {
			int r;
			if (!(r = opus_tags_read(t->data_in.ptr, t->data_in.len))) {
				errlog(t, "opus_tags_read");
				return PHI_ERR;
			}
			ffstr vc = t->data_in;
			ffstr_shift(&vc, r);
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
			return (t->conf.info_only || (t->chain_flags & PHI_FFIRST)) ? PHI_DONE : PHI_OK;

		case 3:
			if (t->audio.ogg_reset) {
				core->metaif->destroy(&t->meta);
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
