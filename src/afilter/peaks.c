/** phiola: Analyze and print audio peaks information.
2019, Simon Zolin */

#include <track.h>
#include <afilter/pcm.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define userlog(t, ...)  phi_userlog(core, NULL, t, __VA_ARGS__)

struct peaks {
	uint channels;
	uint64 total;

	struct {
		double max;
		uint64 clipped;
	} ch[8];
};

static void* peaks_open(phi_track *t)
{
	if (!(t->oaudio.format.interleaved
		&& t->oaudio.format.format == PHI_PCM_FLOAT64
		&& t->oaudio.format.channels <= 8)) {
		errlog(t, "invalid input format");
		return PHI_OPEN_ERR;
	}

	struct peaks *p = phi_track_allocT(t, struct peaks);
	p->channels = t->oaudio.format.channels;
	return p;
}

static void peaks_close(struct peaks *p, phi_track *t)
{
	phi_track_free(t, p);
}

static int peaks_process(struct peaks *p, phi_track *t)
{
	ffsize i, c, samples;

	samples = t->data_in.len / 8 / p->channels;
	p->total += samples;

	const double *data = (void*)t->data_in.ptr;
	for (c = 0;  c != p->channels;  c++) {
		for (i = 0;  i != samples;  i++) {
			double d = data[i * p->channels + c];

			if (d >= 1 || d <= -1)
				p->ch[c].clipped++;

			d = fabs(d);
			if (p->ch[c].max < d)
				p->ch[c].max = d;
		}
	}

	t->data_out = t->data_in;

	if (t->chain_flags & PHI_FFIRST) {
		ffvec buf = {};
		ffvec_addfmt(&buf, "\nPCM peaks (%,U total samples):\n"
			, p->total);

		if (p->total != 0) {
			for (c = 0;  c != p->channels;  c++) {

				double hi = gain_db(p->ch[c].max);
				ffvec_addfmt(&buf, "Channel #%L: max peak: %.2FdB  Clipped: %U\n"
					, c + 1, hi, p->ch[c].clipped);
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
