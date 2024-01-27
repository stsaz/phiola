/**
2015, Simon Zolin
*/

#include "pcm.h"
#include <math.h>

enum CHAN_MASK {
	CHAN_FL = 1,
	CHAN_FR = 2,
	CHAN_FC = 4,
	CHAN_LFE = 8,
	CHAN_BL = 0x10,
	CHAN_BR = 0x20,
	CHAN_SL = 0x40,
	CHAN_SR = 0x80,
};

/** Return channel mask by channels number. */
static uint chan_mask(uint channels)
{
	uint m;
	switch (channels) {
	case 1:
		m = CHAN_FC; break;
	case 2:
		m = CHAN_FL | CHAN_FR; break;
	case 6:
		m = CHAN_FL | CHAN_FR | CHAN_FC | CHAN_LFE | CHAN_BL | CHAN_BR; break;
	case 8:
		m = CHAN_FL | CHAN_FR | CHAN_FC | CHAN_LFE | CHAN_BL | CHAN_BR | CHAN_SL | CHAN_SR; break;
	default:
		return -1;
	}
	return m;
}

#define FF_BIT32(bit)  (1U << (bit))

/** Set gain level for all used channels. */
static int chan_fill_gain_levels(double level[8][8], uint imask, uint omask)
{
	enum {
		FL,
		FR,
		FC,
		LFE,
		BL,
		BR,
		SL,
		SR,
	};

	const double sqrt1_2 = 0.70710678118654752440; // =1/sqrt(2)

	uint equal = imask & omask;
	for (uint c = 0;  c != 8;  c++) {
		if (equal & FF_BIT32(c))
			level[c][c] = 1;
	}

	uint unused = imask & ~omask;

	if (unused & CHAN_FL) {

		if (omask & CHAN_FC) {
			// front stereo -> front center
			level[FC][FL] = sqrt1_2;
			level[FC][FR] = sqrt1_2;

		} else
			return -1;
	}

	if (unused & CHAN_FC) {

		if (omask & CHAN_FL) {
			// front center -> front stereo
			level[FL][FC] = sqrt1_2;
			level[FR][FC] = sqrt1_2;

		} else
			return -1;
	}

	if (unused & CHAN_LFE) {
	}

	if (unused & CHAN_BL) {

		if (omask & CHAN_FL) {
			// back stereo -> front stereo
			level[FL][BL] = sqrt1_2;
			level[FR][BR] = sqrt1_2;

		} else if (omask & CHAN_FC) {
			// back stereo -> front center
			level[FC][BL] = sqrt1_2*sqrt1_2;
			level[FC][BR] = sqrt1_2*sqrt1_2;

		} else
			return -1;
	}

	if (unused & CHAN_SL) {

		if (omask & CHAN_FL) {
			// side stereo -> front stereo
			level[FL][SL] = sqrt1_2;
			level[FR][SR] = sqrt1_2;

		} else if (omask & CHAN_FC) {
			// side stereo -> front center
			level[FC][SL] = sqrt1_2*sqrt1_2;
			level[FC][SR] = sqrt1_2*sqrt1_2;

		} else
			return -1;
	}

	// now gain level can be >1.0, so we normalize it
	for (uint oc = 0;  oc != 8;  oc++) {
		if (!ffbit_test32(&omask, oc))
			continue;

		double sum = 0;
		for (uint ic = 0;  ic != 8;  ic++) {
			sum += level[oc][ic];
		}
		if (sum != 0) {
			for (uint ic = 0;  ic != 8;  ic++) {
				level[oc][ic] /= sum;
			}
		}

		// FFDBG_PRINTLN(10, "channel #%u: %F %F %F %F %F %F %F %F"
		// 	, oc
		// 	, level[oc][0], level[oc][1], level[oc][2], level[oc][3]
		// 	, level[oc][4], level[oc][5], level[oc][6], level[oc][7]);
	}

	return 0;
}

/** Mix (upmix, downmix) channels.
ochan: Output channels number
odata: Output data; float, interleaved

Supported layouts:
1: FC
2: FL+FR
5.1: FL+FR+FC+LFE+BL+BR
7.1: FL+FR+FC+LFE+BL+BR+SL+SR

Examples:

5.1 -> 1:
	FC = FL*0.7 + FR*0.7 + FC*1 + BL*0.5 + BR*0.5

5.1 -> 2:
	FL = FL*1 + FC*0.7 + BL*0.7
	FR = FR*1 + FC*0.7 + BR*0.7
*/
static int _pcm_chan_mix(uint ochan, void *odata, const struct phi_af *inpcm, const void *idata, ffsize samples)
{
	union pcmdata in, out;
	double level[8][8] = {}; // gain level [OUT] <- [IN]
	void *ini[8];
	uint istep, ostep; // intervals between samples of the same channel
	uint ic, oc, ocstm; // channel counters
	uint imask, omask; // channel masks
	ffsize i;

	imask = chan_mask(inpcm->channels);
	omask = chan_mask(ochan);
	if (imask == 0 || omask == 0)
		return -1;

	if (0 != chan_fill_gain_levels(level, imask, omask))
		return -1;

	// set non-interleaved input array
	istep = 1;
	in.pb = (void*)idata;
	if (inpcm->interleaved) {
		pcm_setni(ini, (void*)idata, inpcm->format, inpcm->channels);
		in.pb = (void*)ini;
		istep = inpcm->channels;
	}

	// set interleaved output array
	out.f = odata;
	ostep = ochan;

	ocstm = 0;
	switch (inpcm->format) {
	case PHI_PCM_16:
		for (oc = 0;  oc != 8;  oc++) {

			if (!ffbit_test32(&omask, oc))
				continue;

			for (i = 0;  i != samples;  i++) {
				double sum = 0;
				uint icstm = 0;
				for (ic = 0;  ic != 8;  ic++) {
					if (!ffbit_test32(&imask, ic))
						continue;
					sum += pcm_16le_flt(in.psh[icstm][i * istep]) * level[oc][ic];
					icstm++;
				}
				out.f[ocstm + i * ostep] = pcm_limf(sum);
			}

			if (++ocstm == ochan)
				break;
		}
		break;

	case PHI_PCM_24:
		for (uint oc = 0;  oc != 8;  oc++) {
			if (ffbit_test32(&omask, oc)) {
				for (uint i = 0;  i != samples;  i++) {
					double sum = 0;
					uint icstm = 0;
					for (uint ic = 0;  ic != 8;  ic++) {
						if (ffbit_test32(&imask, ic)) {
							sum += pcm_24_flt(int_ltoh24s(&in.pb[icstm++][i * istep * 3])) * level[oc][ic];
						}
					}
					out.f[ocstm + i * ostep] = pcm_limf(sum);
				}
			}

			if (++ocstm == ochan)
				break;
		}
		break;

	case PHI_PCM_32:
		for (oc = 0;  oc != 8;  oc++) {

			if (!ffbit_test32(&omask, oc))
				continue;

			for (i = 0;  i != samples;  i++) {
				double sum = 0;
				uint icstm = 0;
				for (ic = 0;  ic != 8;  ic++) {
					if (!ffbit_test32(&imask, ic))
						continue;
					sum += pcm_32_flt(in.pin[icstm][i * istep]) * level[oc][ic];
					icstm++;
				}
				out.f[ocstm + i * ostep] = pcm_limf(sum);
			}

			if (++ocstm == ochan)
				break;
		}
		break;

	case PHI_PCM_FLOAT32:
		for (oc = 0;  oc != 8;  oc++) {

			if (!ffbit_test32(&omask, oc))
				continue;

			for (i = 0;  i != samples;  i++) {
				double sum = 0;
				uint icstm = 0;
				for (ic = 0;  ic != 8;  ic++) {
					if (!ffbit_test32(&imask, ic))
						continue;
					sum += in.pf[icstm][i * istep] * level[oc][ic];
					icstm++;
				}
				out.f[ocstm + i * ostep] = pcm_limf(sum);
			}

			if (++ocstm == ochan)
				break;
		}
		break;

	default:
		return -1;
	}

	return 0;
}

#define X(f1, f2) \
	(f1 << 16) | (f2 & 0xffff)

/** Convert PCM samples
Note: sample rate conversion isn't supported. */
/* Algorithm:
If channels don't match, do channel conversion:
  . upmix/downmix: mix appropriate channels with each other.  Requires additional memory buffer.
  . mono: copy data for 1 channel only, skip other channels

If format and "interleaved" flags match for both input and output, just copy the data.
Otherwise, process each channel and sample in a loop.

non-interleaved: data[0][..] - left,  data[1][..] - right
interleaved: data[0,2..] - left */
static inline int pcm_convert(const struct phi_af *outpcm, void *out, const struct phi_af *inpcm, const void *in, ffsize samples)
{
	ffsize i;
	uint ich, nch = inpcm->channels, in_ileaved = inpcm->interleaved;
	union pcmdata from, to;
	void *tmpptr = NULL;
	int r = -1;
	void *ini[8], *oni[8];
	uint istep = 1, ostep = 1;
	uint ifmt;

	from.sh = (void*)in;
	ifmt = inpcm->format;

	to.sh = out;

	if (inpcm->channels > 8 || (outpcm->channels & PHI_PCM_CHMASK) > 8)
		goto done;

	if (inpcm->rate != outpcm->rate)
		goto done;

	if (inpcm->channels != outpcm->channels) {

		nch = outpcm->channels & PHI_PCM_CHMASK;

		if (nch == 1 && (outpcm->channels & ~PHI_PCM_CHMASK) != 0) {
			uint ch = ((outpcm->channels & ~PHI_PCM_CHMASK) >> 4) - 1;
			if (ch > 1)
				goto done;

			if (!inpcm->interleaved) {
				from.psh = from.psh + ch;

			} else {
				ini[0] = from.b + ch * pcm_bits(inpcm->format) / 8;
				from.pb = (void*)ini;
				istep = inpcm->channels;
				in_ileaved = 0;
			}

		} else if ((outpcm->channels & ~PHI_PCM_CHMASK) == 0) {
			if (NULL == (tmpptr = ffmem_alloc(samples * nch * sizeof(float))))
				goto done;

			if (0 != _pcm_chan_mix(nch, tmpptr, inpcm, in, samples))
				goto done;

			if (outpcm->interleaved) {
				from.b = tmpptr;
				in_ileaved = 1;

			} else {
				pcm_setni(ini, tmpptr, PHI_PCM_FLOAT32, nch);
				from.pb = (void*)ini;
				istep = nch;
				in_ileaved = 0;
			}
			ifmt = PHI_PCM_FLOAT32;

		} else
			goto done; // this channel conversion is not supported
	}

	if (ifmt == outpcm->format && istep == 1) {
		// input & output formats are the same, try to copy data directly

		if (in_ileaved != outpcm->interleaved && nch == 1) {
			if (samples == 0)
			{}
			else if (!in_ileaved) {
				// non-interleaved input mono -> interleaved input mono
				from.b = from.pb[0];
			} else {
				// interleaved input mono -> non-interleaved input mono
				ini[0] = from.b;
				from.pb = (void*)ini;
			}
			in_ileaved = outpcm->interleaved;
		}

		if (in_ileaved == outpcm->interleaved) {
			if (samples == 0)
				;
			else if (in_ileaved) {
				// interleaved input -> interleaved output
				ffmem_copy(to.b, from.b, samples * pcm_size(ifmt, nch));
			} else {
				// non-interleaved input -> non-interleaved output
				for (ich = 0;  ich != nch;  ich++) {
					ffmem_copy(to.pb[ich], from.pb[ich], samples * pcm_bits(ifmt)/8);
				}
			}
			r = 0;
			goto done;
		}
	}

	if (in_ileaved) {
		from.pb = pcm_setni(ini, from.b, ifmt, nch);
		istep = nch;
	}

	if (outpcm->interleaved) {
		to.pb = pcm_setni(oni, to.b, outpcm->format, nch);
		ostep = nch;
	}

	switch (X(ifmt, outpcm->format)) {

// uint8
	case X(PHI_PCM_U8, PHI_PCM_16):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ich][i * ostep] = (((int)from.pub[ich][i * istep]) - 127) * 0x100;
			}
		}
		break;

	case X(PHI_PCM_U8, PHI_PCM_32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pin[ich][i * ostep] = (((int)from.pub[ich][i * istep]) - 127) * 0x1000000;
			}
		}
		break;

// int8
	case X(PHI_PCM_8, PHI_PCM_8):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pb[ich][i * ostep] = from.pb[ich][i * istep];
			}
		}
		break;

	case X(PHI_PCM_8, PHI_PCM_16):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ich][i * ostep] = (int)from.pb[ich][i * istep] * 0x100;
			}
		}
		break;

	case X(PHI_PCM_8, PHI_PCM_32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pin[ich][i * ostep] = (int)from.pb[ich][i * istep] * 0x1000000;
			}
		}
		break;

	case X(PHI_PCM_8, PHI_PCM_FLOAT32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pf[ich][i * ostep] = pcm_s8_flt((int)from.pb[ich][i * istep]);
			}
		}
		break;

// int16
	case X(PHI_PCM_16, PHI_PCM_8):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pb[ich][i * ostep] = from.psh[ich][i * istep] / 0x100;
			}
		}
		break;

	case X(PHI_PCM_16, PHI_PCM_16):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ich][i * ostep] = from.psh[ich][i * istep];
			}
		}
		break;

	case X(PHI_PCM_16, PHI_PCM_24):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				int_htol24(&to.pb[ich][i * ostep * 3], (int)from.psh[ich][i * istep] * 0x100);
			}
		}
		break;

	case X(PHI_PCM_16, PHI_PCM_24_4):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pb[ich][i * ostep * 4 + 0] = 0;
				int_htol24(&to.pb[ich][i * ostep * 4 + 1], (int)from.psh[ich][i * istep] * 0x100);
			}
		}
		break;

	case X(PHI_PCM_16, PHI_PCM_32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pin[ich][i * ostep] = (int)from.psh[ich][i * istep] * 0x10000;
			}
		}
		break;

	case X(PHI_PCM_16, PHI_PCM_FLOAT32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pf[ich][i * ostep] = pcm_16le_flt(from.psh[ich][i * istep]);
			}
		}
		break;

	case X(PHI_PCM_16, PHI_PCM_FLOAT64):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pd[ich][i * ostep] = pcm_16le_flt(from.psh[ich][i * istep]);
			}
		}
		break;

// int24
	case X(PHI_PCM_24, PHI_PCM_16):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ich][i * ostep] = ffint_le_cpu24_ptr(&from.pb[ich][i * istep * 3]) / 0x100;
			}
		}
		break;


	case X(PHI_PCM_24, PHI_PCM_24):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				ffmem_copy(&to.pb[ich][i * ostep * 3], &from.pb[ich][i * istep * 3], 3);
			}
		}
		break;

	case X(PHI_PCM_24, PHI_PCM_24_4):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pb[ich][i * ostep * 4 + 0] = 0;
				ffmem_copy(&to.pb[ich][i * ostep * 4 + 1], &from.pb[ich][i * istep * 3], 3);
			}
		}
		break;

	case X(PHI_PCM_24, PHI_PCM_32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pin[ich][i * ostep] = ffint_le_cpu24_ptr(&from.pb[ich][i * istep * 3]) * 0x100;
			}
		}
		break;

	case X(PHI_PCM_24, PHI_PCM_FLOAT32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pf[ich][i * ostep] = pcm_24_flt(int_ltoh24s(&from.pb[ich][i * istep * 3]));
			}
		}
		break;

	case X(PHI_PCM_24, PHI_PCM_FLOAT64):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pd[ich][i * ostep] = pcm_24_flt(int_ltoh24s(&from.pb[ich][i * istep * 3]));
			}
		}
		break;

// int32
	case X(PHI_PCM_32, PHI_PCM_16):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ich][i * ostep] = from.pin[ich][i * istep];
			}
		}
		break;

	case X(PHI_PCM_32, PHI_PCM_24):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				int_htol24(&to.pb[ich][i * ostep * 3], from.pin[ich][i * istep] / 0x100);
			}
		}
		break;

	case X(PHI_PCM_32, PHI_PCM_24_4):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pb[ich][i * ostep * 4 + 0] = 0;
				int_htol24(&to.pb[ich][i * ostep * 4 + 1], from.pin[ich][i * istep] / 0x100);
			}
		}
		break;

	case X(PHI_PCM_32, PHI_PCM_32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pin[ich][i * ostep] = from.pin[ich][i * istep];
			}
		}
		break;

	case X(PHI_PCM_32, PHI_PCM_FLOAT32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pf[ich][i * ostep] = pcm_32_flt(from.pin[ich][i * istep]);
			}
		}
		break;

// float32
	case X(PHI_PCM_FLOAT32, PHI_PCM_16):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ich][i * ostep] = pcm_flt_16le(from.pf[ich][i * istep]);
			}
		}
		break;

	case X(PHI_PCM_FLOAT32, PHI_PCM_24):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				int_htol24(&to.pb[ich][i * ostep * 3], pcm_flt_24(from.pf[ich][i * istep]));
			}
		}
		break;

	case X(PHI_PCM_FLOAT32, PHI_PCM_24_4):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pb[ich][i * ostep * 4 + 0] = 0;
				int_htol24(&to.pb[ich][i * ostep * 4 + 1], pcm_flt_24(from.pf[ich][i * istep]));
			}
		}
		break;

	case X(PHI_PCM_FLOAT32, PHI_PCM_32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pin[ich][i * ostep] = pcm_flt_32(from.pf[ich][i * istep]);
			}
		}
		break;

	case X(PHI_PCM_FLOAT32, PHI_PCM_FLOAT32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pf[ich][i * ostep] = from.pf[ich][i * istep];
			}
		}
		break;

	case X(PHI_PCM_FLOAT32, PHI_PCM_FLOAT64):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pd[ich][i * ostep] = from.pf[ich][i * istep];
			}
		}
		break;

// float64
	case X(PHI_PCM_FLOAT64, PHI_PCM_16):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.psh[ich][i * ostep] = pcm_flt_16le(from.pd[ich][i * istep]);
			}
		}
		break;

	case X(PHI_PCM_FLOAT64, PHI_PCM_24):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				int_htol24(&to.pb[ich][i * ostep * 3], pcm_flt_24(from.pd[ich][i * istep]));
			}
		}
		break;

	case X(PHI_PCM_FLOAT64, PHI_PCM_32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pin[ich][i * ostep] = pcm_flt_32(from.pd[ich][i * istep]);
			}
		}
		break;

	case X(PHI_PCM_FLOAT64, PHI_PCM_FLOAT32):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pf[ich][i * ostep] = from.pd[ich][i * istep];
			}
		}
		break;

	case X(PHI_PCM_FLOAT64, PHI_PCM_FLOAT64):
		for (ich = 0;  ich != nch;  ich++) {
			for (i = 0;  i != samples;  i++) {
				to.pd[ich][i * ostep] = from.pd[ich][i * istep];
			}
		}
		break;

	default:
		goto done;
	}
	r = 0;

done:
	ffmem_free(tmpptr);
	return r;
}

#undef X

