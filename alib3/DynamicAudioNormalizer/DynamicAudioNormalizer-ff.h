/** libDynamicAudioNormalizer interface
2018, Simon Zolin */

#pragma once
#include <stdlib.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
	#include <sys/types.h>
#endif

struct dynanorm_conf {
	unsigned int channels;
	unsigned int sampleRate;
	unsigned int frameLenMsec;
	unsigned int filterSize;
	double peakValue;
	double maxAmplification;
	double targetRms;
	double compressFactor;
	unsigned int channelsCoupled :1;
	unsigned int enableDCCorrection :1;
	unsigned int altBoundaryMode :1;
};

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT void dynanorm_init(struct dynanorm_conf *conf);

/**
Return 0 on success. */
_EXPORT int dynanorm_open(void **c, struct dynanorm_conf *conf);

_EXPORT void dynanorm_close(void *c);

/** Reset buffer */
_EXPORT void dynanorm_reset(void *c);

/** Process audio data.
in: input audio data
 NULL: Input data is complete, start flushing the bufferred audio
samples: input samples;  on return contains number of samples processed.
Return the number of output samples
 <0 on error. */
_EXPORT ssize_t dynanorm_process(void *cc, const double *const *in, size_t *samples, double **out, size_t max_samples);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
