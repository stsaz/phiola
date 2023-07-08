/**
2015, Simon Zolin
*/

#include "pcm.h"

static inline int pcm_gain(const struct phi_af *pcm, float gain, const void *in, void *out, uint samples)
{
	uint i, ich, step = 1, nch = pcm->channels;
	void *ini[8], *oni[8];
	union pcmdata from, to;

	if (gain == 1)
		return 0;

	if (pcm->channels > 8)
		return -1;

	from.sh = (void*)in;
	to.sh = out;

	if (pcm->interleaved) {
		from.pb = pcm_setni(ini, from.b, pcm->format, nch);
		to.pb = pcm_setni(oni, to.b, pcm->format, nch);
		step = nch;
	}

	switch (pcm->format) {
	case PHI_PCM_8:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pb[ich][i * step] = pcm_flt_8(pcm_8_flt(from.pb[ich][i * step]) * gain);
			}
		}
		break;

	case PHI_PCM_16:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ich][i * step] = pcm_flt_16le(pcm_16le_flt(from.psh[ich][i * step]) * gain);
			}
		}
		break;

	case PHI_PCM_24:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				int n = int_ltoh24s(&from.pb[ich][i * step * 3]);
				int_htol24(&to.pb[ich][i * step * 3], pcm_flt_24(pcm_24_flt(n) * gain));
			}
		}
		break;

	case PHI_PCM_32:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pin[ich][i * step] = pcm_flt_32(pcm_32_flt(from.pin[ich][i * step]) * gain);
			}
		}
		break;

	case PHI_PCM_FLOAT32:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pf[ich][i * step] = from.pf[ich][i * step] * gain;
			}
		}
		break;

	case PHI_PCM_FLOAT64:
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pd[ich][i * step] = from.pd[ich][i * step] * gain;
			}
		}
		break;

	default:
		return -1;
	}

	return 0;
}
