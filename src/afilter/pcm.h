/** PCM.
2015, Simon Zolin */

#pragma once
#include <math.h>

union pcmdata {
	char *b;
	short *sh;
	int *in;
	float *f;
	double *d;

	u_char **pub;
	char **pb;
	short **psh;
	int **pin;
	float **pf;
	double **pd;
};

#define PHI_PCM_CHMASK 0x0f

#ifndef pcm_bits
/** Get bits per sample for one channel. */
#define pcm_bits(fmt)  ((fmt) & 0xff)
#endif

/** Get size of 1 sample (in bytes). */
#define pcm_size(format, channels)  (pcm_bits(format) / 8 * (channels))
#ifndef pcm_size1
#define pcm_size1(pcm)  pcm_size((pcm)->format, (pcm)->channels)
#endif

/** Convert between samples and time. */
#define pcm_samples(time_ms, rate)   ((uint64)(time_ms) * (rate) / 1000)
#define pcm_time(samples, rate)   ((uint64)(samples) * 1000 / (rate))

/** Convert PCM signed 8-bit sample to FLOAT */
#define pcm_s8_flt(sh)  ((double)(sh) * (1 / 128.0))

/** Convert 16LE sample to FLOAT. */
#define pcm_16le_flt(sh)  ((double)(sh) * (1 / 32768.0))

/** Convert volume knob position to dB value. */
#define vol2db(pos, db_min) \
	(((pos) != 0) ? (log10(pos) * (db_min)/2 /*log10(100)*/ - (db_min)) : -100)

#define vol2db_inc(pos, pos_max, db_max) \
	(pow(10, (double)(pos) / (pos_max)) / 10 * (db_max))

/* gain = 10 ^ (db / 20) */
#define db_gain(db)  pow(10, (double)(db) / 20)
#define gain_db(gain)  (log10(gain) * 20)

static inline int int_ltoh24s(const void *p)
{
	const ffbyte *b = (ffbyte*)p;
	uint n = ((uint)b[2] << 16) | ((uint)b[1] << 8) | b[0];
	if (n & 0x00800000)
		n |= 0xff000000;
	return n;
}

static inline void int_htol24(void *p, uint n)
{
	ffbyte *o = (ffbyte*)p;
	o[0] = (ffbyte)n;
	o[1] = (ffbyte)(n >> 8);
	o[2] = (ffbyte)(n >> 16);
}

#ifdef FF_SSE2
	#include <emmintrin.h>

#elif defined FF_ARM64
	#include <arm_neon.h>
#endif

#ifdef FF_ARM64
typedef float64x2_t __m128d;
/** m128[0:63] = f64; m128[64:127] = 0 */
static inline __m128d neon_load_sd(const double *d)
{
	return vsetq_lane_f64(*d, vdupq_n_f64(0), 0);
}
#endif

/** Convert FP number to integer. */
static inline int int_ftoi(double d)
{
	int r;

#if defined FF_SSE2
	r = _mm_cvtsd_si32(_mm_load_sd(&d));

#elif defined FF_ARM64
	r = vgetq_lane_f64(vrndiq_f64(neon_load_sd(&d)), 0);

#else
	r = (int)((d < 0) ? d - 0.5 : d + 0.5);
#endif

	return r;
}

#define max8f  (128.0)

static inline short pcm_flt_8(float f)
{
	double d = f * max8f;
	if (d < -max8f)
		return -0x80;
	else if (d > max8f - 1)
		return 0x7f;
	return int_ftoi(d);
}

#define pcm_8_flt(sh)  ((float)(sh) * (1 / max8f))

#define max16f  (32768.0)

static inline int _int_lim16(int i)
{
	if (i < -0x8000)
		i = -0x8000;
	else if (i > 0x7fff)
		i = 0x7fff;
	return i;
}

static inline short pcm_flt_16le(double f)
{
	double d = f * max16f;
	if (d < -max16f)
		return -0x8000;
	else if (d > max16f - 1)
		return 0x7fff;
	return int_ftoi(d);
}

#define max24f  (8388608.0)

static inline int pcm_flt_24(double f)
{
	double d = f * max24f;
	if (d < -max24f)
		return -0x800000;
	else if (d > max24f - 1)
		return 0x7fffff;
	return int_ftoi(d);
}

#define pcm_24_flt(n)  ((double)(n) * (1 / max24f))

#define max32f  (2147483648.0)

static inline int pcm_flt_32(double d)
{
	d *= max32f;
	if (d < -max32f)
		return -0x80000000;
	else if (d > max32f - 1)
		return 0x7fffffff;
	return int_ftoi(d);
}

#define pcm_32_flt(n)  ((double)(n) * (1 / max32f))

static inline double pcm_limf(double d)
{
	if (d > 1.0)
		return 1.0;
	else if (d < -1.0)
		return -1.0;
	return d;
}

/** Set non-interleaved array from interleaved data. */
static inline char** pcm_setni(void **ni, void *b, uint fmt, uint nch)
{
	for (uint i = 0;  i != nch;  i++) {
		ni[i] = (char*)b + i * pcm_bits(fmt) / 8;
	}
	return (char**)ni;
}

/** Q7.8 number <-> float (max. value = 127.99999999 ~= 128) */
static inline int Q78_float(int n, double *dst) {
	if (n < -0x8000 || n > 0x8000)
		return -1;
	*dst = (double)n / 256; // 256 = 2^8
	return 0;
}
static inline int Q78_from_float(int *dst, double d) {
	*dst = round(d * 256);
	return (*dst < -0x8000 || *dst > 0x8000);
}

// -18: RG target
// -23: R128 target
// -18 -> -23 = -5
// -23 -> -18 = +5
#define RG_R128(rg2)  ((rg2) - 5)
#define RG_from_R128(r128)  ((r128) + 5)
