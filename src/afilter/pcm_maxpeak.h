/**
2015, Simon Zolin
*/

#include "pcm.h"

/** Find the highest peak value */
static inline int pcm_maxpeak(const struct phi_af *fmt, const void *data, ffsize samples, double *maxpeak)
{
	double max_f = 0.0;
	uint max_sh = 0;
	uint ich, nch = fmt->channels, step = 1;
	ffsize i;
	void *ni[8];
	union pcmdata d;
	d.sh = (void*)data;

	if (fmt->channels > 8)
		return 1;

	if (fmt->interleaved) {
		d.pb = pcm_setni(ni, d.b, fmt->format, nch);
		step = nch;
	}

	switch (fmt->format) {

	case PHI_PCM_16:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				uint sh = ffint_abs(d.psh[ich][i * step]);
				if (max_sh < sh)
					max_sh = sh;
			}
		}
		max_f = pcm_16le_flt(max_sh);
		break;

	case PHI_PCM_24:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				int n = int_ltoh24s(&d.pb[ich][i * step * 3]);
				uint u = ffint_abs(n);
				if (max_sh < u)
					max_sh = u;
			}
		}
		max_f = pcm_24_flt(max_sh);
		break;

	case PHI_PCM_32:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				int n = ffint_le_cpu32_ptr(&d.pin[ich][i * step]);
				uint u = ffint_abs(n);
				if (max_sh < u)
					max_sh = u;
			}
		}
		max_f = pcm_32_flt(max_sh);
		break;

	case PHI_PCM_FLOAT32:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				double f = ffint_abs(d.pf[ich][i * step]);
				if (max_f < f)
					max_f = f;
			}
		}
		break;

	default:
		return 1;
	}

	if (maxpeak)
		*maxpeak = max_f;
	return 0;
}
