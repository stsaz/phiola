/** libvorbis, libvorbisenc interface
2016, Simon Zolin */

#pragma once
#include <stdint.h>

#ifdef WIN32
	#define _EXPORT  __declspec(dllexport)
#else
	#define _EXPORT  __attribute__((visibility("default")))
#endif

typedef struct vorbis_ctx vorbis_ctx;

#ifndef VORB_EXP
/* ogg/ogg.h */
typedef struct {
	unsigned char *packet;
	long bytes;
	long b_o_s;
	long e_o_s;
	long long granulepos;
	long long packetno;
} ogg_packet;
#endif

#ifdef __cplusplus
extern "C" {
#endif

_EXPORT const char* vorbis_errstr(int e);

/** Get vendor string needed for Vorbis comments packet when encoding. */
_EXPORT const char* vorbis_vendor(void);


/** Initialize decoder.
Call twice:
 . with Vorbis header packet;
 . with Vorbis book packet. */
_EXPORT int vorbis_decode_init(vorbis_ctx **pv, const ogg_packet *pkt);

_EXPORT void vorbis_decode_free(vorbis_ctx *v);

/** Decode one packet.
samples: the number of output samples.
Return the number of audio samples decoded;
 <0 on error. */
_EXPORT int vorbis_decode(vorbis_ctx *v, ogg_packet *pkt, const float ***pcm);


typedef struct vorbis_encode_params {
	unsigned int channels;
	unsigned int rate;
	float quality;
} vorbis_encode_params;

/** Initialize encoder. */
_EXPORT int vorbis_encode_create(vorbis_ctx **pv, vorbis_encode_params *params, ogg_packet *pkt_hdr, ogg_packet *pkt_book);

_EXPORT void vorbis_encode_free(vorbis_ctx *v);

/** Compress PCM data and return OGG packet when ready.
samples: number of input samples; -1: end of stream.
Return 1 if packet is complete;
 0 if more data is needed;
 <0: error. */
_EXPORT int vorbis_encode(vorbis_ctx *v, const float* const *pcm, int samples, ogg_packet *pkt);

#ifdef __cplusplus
}
#endif

#undef _EXPORT
