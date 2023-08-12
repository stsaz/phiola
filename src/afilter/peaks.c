/** Analyze and print audio peaks information.
2019, Simon Zolin */

#include <track.h>
#include <afilter/pcm.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define userlog(t, ...)  phi_userlog(core, NULL, t, __VA_ARGS__)

/** Fast CRC32 implementation using 8k table. */
extern uint crc32(const void *buf, ffsize size, uint crc);

struct peaks {
	uint state;
	uint nch;
	uint64 total;

	struct {
		uint crc;
		uint high;
		uint64 sum;
		uint64 clipped;
	} ch[8];
	uint do_crc :1;
};

static void* peaks_open(phi_track *t)
{
	struct peaks *p;
	if (t->oaudio.format.channels > FF_COUNT(p->ch)) {
		errlog(t, "invalid channels");
		return PHI_OPEN_ERR;
	}

	p = ffmem_new(struct peaks);
	p->nch = t->oaudio.format.channels;
	p->do_crc = t->conf.afilter.peaks_crc;
	return p;
}

static void peaks_close(struct peaks *p, phi_track *t)
{
	ffmem_free(p);
}

static int peaks_process(struct peaks *p, phi_track *t)
{
	ffsize i, ich, samples;

	switch (p->state) {
	case 0:
		t->oaudio.conv_format.interleaved = 0;
		t->oaudio.conv_format.format = PHI_PCM_16;
		p->state = 1;
		return PHI_MORE;

	case 1:
		if (t->oaudio.format.interleaved
			|| t->oaudio.format.format != PHI_PCM_16) {
			errlog(t, "input must be non-interleaved 16LE PCM");
			return PHI_ERR;
		}
		p->state = 2;
		break;
	}

	samples = t->data_in.len / (sizeof(short) * p->nch);
	p->total += samples;

	void **datani = (void**)t->data_in.ptr;
	for (ich = 0;  ich != p->nch;  ich++) {
		for (i = 0;  i != samples;  i++) {
			int sh = ((short**)t->data_in.ptr)[ich][i];

			if (sh == 0x7fff || sh == -0x8000)
				p->ch[ich].clipped++;

			if (sh < 0)
				sh = -sh;

			if (p->ch[ich].high < (uint)sh)
				p->ch[ich].high = sh;

			p->ch[ich].sum += sh;
		}

		if (samples != 0 && p->do_crc)
			p->ch[ich].crc = crc32(datani[ich], t->data_in.len / p->nch, p->ch[ich].crc);
	}

	t->data_out = t->data_in;

	if (t->chain_flags & PHI_FFIRST) {
		ffvec buf = {};
		ffvec_addfmt(&buf, "\nPCM peaks (%,U total samples):\n"
			, p->total);

		if (p->total != 0) {
			for (ich = 0;  ich != p->nch;  ich++) {

				double hi = gain_db(pcm_16le_flt(p->ch[ich].high));
				double avg = gain_db(pcm_16le_flt(p->ch[ich].sum / p->total));
				ffvec_addfmt(&buf, "Channel #%L: highest peak:%.2FdB, avg peak:%.2FdB.  Clipped: %U (%.4F%%).  CRC:%08xu\n"
					, ich + 1, hi, avg
					, p->ch[ich].clipped, ((double)p->ch[ich].clipped * 100 / p->total)
					, p->ch[ich].crc);
			}
		}

		userlog(t, "%S", &buf);
		ffvec_free(&buf);
		return PHI_DONE;
	}
	return PHI_OK;
}

const phi_filter phi_peaks = {
	peaks_open, (void*)peaks_close, (void*)peaks_process,
	"peaks"
};
