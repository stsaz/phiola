/** phiola: .ogg write
2015,2021, Simon Zolin */

#include <avpack/ogg-write.h>


struct ogg_w {
	oggwrite og;
	ffvec pktbuf;
	ffstr pkt, in;
	uint state;
	uint64 pos_start; // starting position of the first data page
	uint64 last_pos;
	uint64 total;
	uint pos_start_set :1;
};

static void* ogg_w_open(phi_track *t)
{
	struct ogg_w *o = phi_track_allocT(t, struct ogg_w);
	return o;
}

static void ogg_w_close(void *ctx, phi_track *t)
{
	struct ogg_w *o = ctx;
	oggwrite_close(&o->og);
	ffvec_free(&o->pktbuf);
	phi_track_free(t, o);
}

static int pkt_write(struct ogg_w *o, phi_track *t, ffstr *in, ffstr *out, uint64 endpos, uint flags)
{
	dbglog(t, "oggwrite_process(): size:%L  endpos:%U flags:%u", in->len, endpos, flags);
	int r = oggwrite_process(&o->og, in, out, endpos, flags);

	switch (r) {

	case OGGWRITE_DONE:
		verblog(t, "OGG: packets:%U, pages:%U, overhead: %.2F%%"
			, (int64)o->og.stat.npkts, (int64)o->og.stat.npages
			, (double)o->og.stat.total_ogg * 100 / (o->og.stat.total_payload + o->og.stat.total_ogg));
		return PHI_LASTOUT;

	case OGGWRITE_DATA:
		break;

	case OGGWRITE_MORE:
		return PHI_MORE;

	default:
		errlog(t, "oggwrite_process() failed");
		return PHI_ERR;
	}

	o->total += out->len;
	dbglog(t, "output: %L bytes (%U), page:%U, end-pos:%D"
		, out->len, o->total, (int64)o->og.stat.npages-1, o->og.page_startpos);
	return PHI_DATA;
}

/*
Per-packet writing ("end-pos" value is optional):
. while "start-pos" == 0: write packet and flush
. Optionally, write packet #2 with empty Opus tags
. write the cached packet with "end-pos" = "current start-pos"
. store the input packet in temp. buffer
. ask for more data; if no more data:
  . write the input packet with "end-pos" = "input start-pos" +1; flush and exit

OGG->OGG copying doesn't require temp. buffer.
*/
static int ogg_w_encode(void *ctx, phi_track *t)
{
	enum { I_INIT, I_PKT, I_OPUS_TAGS, I_PAGE_EXACT };
	struct ogg_w *o = ctx;
	int r;
	uint flags = 0;
	uint64 endpos;

	if (t->chain_flags & PHI_FFWD) {
		o->in = t->data_in;
	}

	for (;;) {
		switch (o->state) {

		case I_INIT: {
			uint max_page_samples;
			if (t->oaudio.ogg_copy) {
				max_page_samples = 0; // ogg->ogg copy must replicate the pages exactly
				o->state = I_PAGE_EXACT;
			} else {
				max_page_samples = (t->oaudio.format.rate) ? t->oaudio.format.rate : 48000;
				o->pkt = o->in;
				o->in.len = 0;
				o->state = I_PKT;
			}

			oggwrite_create(&o->og, core->rand(), max_page_samples);
			continue;
		}

		case I_PKT:
			if (!o->pos_start_set && t->audio.pos) {
				o->pos_start_set = 1;
				o->pos_start = t->audio.pos;
			}

			endpos = t->audio.pos - o->pos_start; // end-pos (for previous packet) = start-pos of this packet
			if (o->og.stat.npkts == 0) {
				endpos = 0;
				if (t->oaudio.ogg_gen_opus_tag)
					o->state = I_OPUS_TAGS;
			}
			if (endpos == 0)
				flags = OGGWRITE_FFLUSH;
			if ((t->chain_flags & PHI_FFIRST) && o->in.len == 0)
				flags = OGGWRITE_FLAST;

			r = pkt_write(o, t, &o->pkt, &t->data_out, endpos, flags);
			if (r == PHI_MORE) {
				if (t->chain_flags & PHI_FFIRST) {
					if (t->audio.total != ~0ULL && endpos < t->audio.total)
						endpos = t->audio.total - o->pos_start;
					else
						endpos++; // we don't know the packet's audio length -> can't set the real end-pos value
					return pkt_write(o, t, &o->in, &t->data_out, endpos, OGGWRITE_FLAST);
				}
				o->pktbuf.len = 0;
				ffvec_add2(&o->pktbuf, &o->in, 1); // store the current data packet
				ffstr_set2(&o->pkt, &o->pktbuf);
			}
			return r;

		case I_OPUS_TAGS: {
			o->state = I_PKT;
			static const char opus_tags[] = "OpusTags\x08\x00\x00\x00" "datacopy\x00\x00\x00\x00";
			ffstr in;
			ffstr_set(&in, opus_tags, sizeof(opus_tags)-1);
			return pkt_write(o, t, &in, &t->data_out, 0, OGGWRITE_FFLUSH);
		}

		case I_PAGE_EXACT:
			if (t->oaudio.ogg_granule_pos != o->last_pos && o->og.page.nsegments != 0) {
				// This packet begins a new page -> flush the current page
				ffstr empty = {};
				return pkt_write(o, t, &empty, &t->data_out, 0, OGGWRITE_FFLUSH);
			}

			if (!o->pos_start_set && t->oaudio.ogg_granule_pos) {
				o->pos_start_set = 1;
				o->pos_start = t->audio.pos;
			}

			endpos = t->oaudio.ogg_granule_pos - o->pos_start;
			if (t->oaudio.ogg_granule_pos == 0)
				flags = OGGWRITE_FFLUSH;
			if (t->chain_flags & PHI_FFIRST) {
				flags = OGGWRITE_FLAST;
				endpos = o->last_pos - o->pos_start + 1; // we don't know the packet's audio length -> can't set the real end-pos value
			}
			o->last_pos = t->oaudio.ogg_granule_pos;

			r = pkt_write(o, t, &o->in, &t->data_out, endpos, flags);
			return r;
		}
	}
}

const phi_filter phi_ogg_copy = {
	ogg_w_open, ogg_w_close, ogg_w_encode,
	"ogg-copy"
};
