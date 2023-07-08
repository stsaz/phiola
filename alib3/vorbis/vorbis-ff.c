/** libvorbis, libvorbisenc interface
2016, Simon Zolin */

#include <vorbis/codec.h>
#include "vorbis-ff.h"
#include <stdlib.h>

static const char* const vorb_errstr[] = {
	"", /*OV_EREAD*/
	"internal error; indicates a bug or memory corruption", /*OV_EFAULT*/
	"unimplemented; not supported by this version of the library", /*OV_EIMPL*/
	"invalid parameter", /*OV_EINVAL*/
	"the packet is not a Vorbis header packet", /*OV_ENOTVORBIS*/
	"error interpreting the packet", /*OV_EBADHEADER*/
	"", /*OV_EVERSION*/
	"the packet is not an audio packet", /*OV_ENOTAUDIO*/
	"there was an error in the packet", /*OV_EBADPACKET*/
	"", /*OV_EBADLINK*/
	"", /*OV_ENOSEEK*/
};

const char* vorbis_errstr(int e)
{
	e = -(e - (OV_EREAD));
	if ((unsigned int)e > sizeof(vorb_errstr) / sizeof(*vorb_errstr))
		return "unknown error";
	return vorb_errstr[e];
}


struct vorbis_ctx {
	vorbis_info info;
	vorbis_dsp_state ds;
	vorbis_block blk;
};

int vorbis_decode_init(vorbis_ctx **pv, const ogg_packet *pkt)
{
	int r;
	vorbis_ctx *v;

	if (*pv == NULL) {
		if (NULL == (v = calloc(1, sizeof(vorbis_ctx))))
			return OV_EFAULT;
		vorbis_info_init(&v->info);

		if (0 != (r = vorbis_synthesis_headerin(&v->info, NULL, (void*)pkt)))
			goto err;

		*pv = v;
		return 0;
	}

	v = *pv;

	if (0 != (r = vorbis_synthesis_headerin(&v->info, NULL, (void*)pkt)))
		goto err;

	if (0 != (r = vorbis_synthesis_init(&v->ds, &v->info)))
		goto err;

	vorbis_block_init(&v->ds, &v->blk);
	return 0;

err:
	free(v);
	*pv = NULL;
	return r;
}

void vorbis_decode_free(vorbis_ctx *v)
{
	vorbis_block_clear(&v->blk);
	vorbis_dsp_clear(&v->ds);
	vorbis_info_clear(&v->info);
	free(v);
}

int vorbis_decode(vorbis_ctx *v, ogg_packet *pkt, const float ***pcm)
{
	int r;

	if (0 != (r = vorbis_synthesis(&v->blk, pkt)))
		return r;

	vorbis_synthesis_blockin(&v->ds, &v->blk);

	r = vorbis_synthesis_pcmout(&v->ds, (float***)pcm);
	vorbis_synthesis_read(&v->ds, r);
	return r;
}
