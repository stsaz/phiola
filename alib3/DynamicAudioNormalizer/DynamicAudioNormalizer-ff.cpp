/** libDynamicAudioNormalizer interface
2018, Simon Zolin */

#include "DynamicAudioNormalizer-ff.h"
#include <DynamicAudioNormalizer.h>
#include <string.h>


void dynanorm_init(struct dynanorm_conf *conf)
{
	memset(conf, 0, sizeof(*conf));
	conf->frameLenMsec = 500;
	conf->filterSize = 31;
	conf->peakValue = 0.95;
	conf->maxAmplification = 10.0;
	conf->targetRms = 0.0;
	conf->compressFactor = 0.0;
	conf->channelsCoupled = true;
	conf->enableDCCorrection = false;
	conf->altBoundaryMode = false;
}

int dynanorm_open(void **pc, struct dynanorm_conf *conf)
{
	MDynamicAudioNormalizer *c = new MDynamicAudioNormalizer(
		conf->channels,
		conf->sampleRate,
		conf->frameLenMsec,
		conf->filterSize,
		conf->peakValue,
		conf->maxAmplification,
		conf->targetRms,
		conf->compressFactor,
		conf->channelsCoupled,
		conf->enableDCCorrection,
		conf->altBoundaryMode,
		NULL
		);
	if (c == NULL)
		return -1;
	if (!c->initialize())
		return -1;
	*pc = c;
	return 0;
}

void dynanorm_close(void *cc)
{
	MDynamicAudioNormalizer *c = (MDynamicAudioNormalizer*)cc;
	delete c;
}

void dynanorm_reset(void *cc)
{
	MDynamicAudioNormalizer *c = (MDynamicAudioNormalizer*)cc;
	c->reset();
}

static size_t ffmin(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

ssize_t dynanorm_process(void *cc, const double *const *in, size_t *samples, double **out, size_t max_samples)
{
	MDynamicAudioNormalizer *c = (MDynamicAudioNormalizer*)cc;
	int64_t wr = 0;
	if (in == NULL) {
		if (!c->flushBuffer(out, max_samples, wr))
			return -1;
	} else {
		size_t n = ffmin(*samples, max_samples);
		if (!c->process(in, out, n, wr))
			return -1;
		*samples = n;
	}
	return wr;
}


#include <Version.h>
const unsigned int DYNAUDNORM_NS::VERSION_MAJOR =  2;
const unsigned int DYNAUDNORM_NS::VERSION_MINOR = 10;
const unsigned int DYNAUDNORM_NS::VERSION_PATCH =  0;
const char *const DYNAUDNORM_NS::BUILD_DATE = "";
const char *const DYNAUDNORM_NS::BUILD_TIME = "";
const char *const DYNAUDNORM_NS::BUILD_COMPILER = "";
const char *const DYNAUDNORM_NS::BUILD_ARCH = "";

#include <Logging.h>
DYNAUDNORM_NS::LoggingCallback *DYNAUDNORM_NS::setLoggingHandler(LoggingCallback *const callback){return NULL;}
void DYNAUDNORM_NS::postLogMessage(const int &logLevel, const char *const message, ...){}
