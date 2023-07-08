/**
Simon Zolin, 2016 */

#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include "vorbis-ff.h"
#include <stdlib.h>
#include <memory.h>


struct vorbis_ctx {
	vorbis_info info;
	vorbis_dsp_state ds;
	vorbis_block blk;
};

int vorbis_encode_create(vorbis_ctx **pv, vorbis_encode_params *params, ogg_packet *pkt_hdr, ogg_packet *pkt_book)
{
	int r;
	vorbis_ctx *v;

	if (NULL == (v = calloc(1, sizeof(vorbis_ctx))))
		return OV_EFAULT;

	vorbis_info_init(&v->info);

	if (0 != (r = vorbis_encode_init_vbr(&v->info, params->channels, params->rate, params->quality))) {
		free(v);
		return r;
	}

	vorbis_analysis_init(&v->ds, &v->info);
	vorbis_block_init(&v->ds, &v->blk);

	if (0 != (r = vorbis_analysis_headerout(&v->ds, NULL, pkt_hdr, NULL, pkt_book))) {
		vorbis_encode_free(v);
		return r;
	}

	*pv = v;
	return 0;
}

void vorbis_encode_free(vorbis_ctx *v)
{
	vorbis_block_clear(&v->blk);
	vorbis_dsp_clear(&v->ds);
	vorbis_info_clear(&v->info);
	free(v);
}

int vorbis_encode(vorbis_ctx *v, const float* const *pcm, int samples, ogg_packet *pkt)
{
	int r;
	r = vorbis_analysis_blockout(&v->ds, &v->blk);
	if (r == 0) {
		if (samples == 0)
			return 0;
		else if (samples > 0) {
			float **vpcm = vorbis_analysis_buffer(&v->ds, samples);
			for (unsigned int i = 0;  i != (unsigned int)v->info.channels;  i++) {
				memcpy(vpcm[i], pcm[i], samples * sizeof(float));
			}
		}
		// (samples <= 0) is an end-of-stream indicator for vorbis_analysis_wrote()

		vorbis_analysis_wrote(&v->ds, samples);
		r = vorbis_analysis_blockout(&v->ds, &v->blk);
	}

	if (r < 0)
		return r;
	else if (r == 0)
		return 0;

	if (0 != (r = vorbis_analysis(&v->blk, pkt)))
		return r;
	return 1;
}
