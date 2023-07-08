/** libALAC interface
2016, Simon Zolin */

#include "ALAC-ff.h"
#include <ALACDecoder.h>
#include <ALACBitUtilities.h>

struct alac_ctx {
	ALACDecoder a;
	struct BitBuffer bbuf;
};

alac_ctx* alac_init(const char *magic_cookie, size_t len)
{
	alac_ctx *a;

	a = new alac_ctx;

	BitBufferInit(&a->bbuf, NULL, 0);
	if (ALAC_noErr != a->a.Init((void*)magic_cookie, len)) {
		delete a;
		return NULL;
	}

	return a;
}

void alac_free(alac_ctx *a)
{
	delete a;
}

int alac_decode(alac_ctx *a, const char *data, size_t len, void *pcm)
{
	unsigned int samples = 0;

	BitBufferInit(&a->bbuf, (unsigned char*)data, len);

	while (a->bbuf.cur != a->bbuf.end) {
		int r = a->a.Decode(&a->bbuf, (unsigned char*)pcm, a->a.mConfig.frameLength, a->a.mConfig.numChannels, &samples);
		if (r != 0)
			return r;
	}

	return samples;
}
