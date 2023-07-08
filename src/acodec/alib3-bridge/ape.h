/** APE (MAC).
2015, Simon Zolin */

/*
(DESC HDR) SEEK_TBL  [WAV_HDR]
ver <= 3.97: HDR  [WAV_HDR]  SEEK_TBL
*/

#pragma once
#include <afilter/pcm.h>
#include <avpack/apetag.h>
#include <avpack/id3v1.h>
#include <MAC/MAC-ff.h>

enum FFAPE_R {
	FFAPE_RDATA,
	FFAPE_RERR,
	FFAPE_RMORE,
	FFAPE_RHDR,
};

typedef struct ffape_info {
	struct phi_af fmt;
	ushort version;
	ushort comp_level_orig;
	ffbyte comp_level;
	uint seekpoints;
	uint frame_blocks;
	uint64 total_samples;
	ffbyte md5[16];
	uint wavhdr_size;
} ffape_info;

typedef struct ffape {
	ffape_info info;
	uint64 total_size;
	uint64 off;
	uint64 froff;

	struct ape_decoder *ap;
	void *pcm;
	uint block_samples;
	uint64 cursample;
	uint64 seek_sample;

	int err;
	uint init;
} ffape;

static inline void ffape_open(ffape *a)
{
}

static inline void ffape_close(ffape *a)
{
	if (a->ap != NULL)
		ape_decode_free(a->ap),  a->ap = NULL;
	ffmem_free(a->pcm);  a->pcm = NULL;
}

static inline void ffape_seek(ffape *a, uint64 sample)
{
	a->seek_sample = sample;
}

#define ffape_totalsamples(a)  ((a)->info.total_samples)

#define ffape_cursample(a)  ((a)->cursample - (a)->block_samples)

struct ape_desc {
	char id[4]; // "MAC "
	ffbyte ver[2]; // = x.xx * 1000.  >=3.98
	ffbyte skip[2];

	ffbyte desc_size[4];
	ffbyte hdr_size[4];
	ffbyte seektbl_size[4];
	ffbyte wavhdr_size[4];
	ffbyte unused[3 * 4];
	ffbyte md5[16];
};

enum APE_FLAGS {
	APE_F8BIT = 1,
	APE_FPEAK = 4,
	APE_F24BIT = 8,
	APE_FNSEEKEL = 0x10,
	APE_FNOWAVHDR = 0x20,
};

struct ape_hdr {
	ffbyte comp_level[2]; // 1000..5000
	ffbyte flags[2]; // enum APE_FLAGS

	ffbyte frame_blocks[4];
	ffbyte lastframe_blocks[4];
	ffbyte total_frames[4];

	ffbyte bps[2];
	ffbyte channels[2];
	ffbyte rate[4];
};

struct ape_hdr_old {
	char id[4];
	ffbyte ver[2]; // <=3.97
	ffbyte comp_level[2];
	ffbyte flags[2]; // enum APE_FLAGS

	ffbyte channels[2];
	ffbyte rate[4];

	ffbyte wavhdr_size[4];
	ffbyte unused[4];

	ffbyte total_frames[4];
	ffbyte lastframe_blocks[4];

	//ffbyte peak_level[4];
	//ffbyte seekpoints[4];
	//ffbyte wavhdr[wavhdr_size];
};

const char ffape_comp_levelstr[][8] = { "", "fast", "normal", "high", "x-high", "insane" };

enum APE_E {
	APE_EOK,
	APE_EHDR,
	APE_EFMT,
	APE_ESMALL,

	APE_ESYS,
};

static const char *const _ffape_errs[] = {
	"",
	"invalid header",
	"unsupported format",
	"too small input data",
};

const char* ffape_errstr(ffape *a)
{
	if (a->err == APE_ESYS)
		return "no memory";
	else if (a->err < 0)
		return ape_errstr(a->err);

	return _ffape_errs[a->err];
}

static int _ape_parse_old(ffape_info *info, const char *data, size_t len)
{
	const struct ape_hdr_old *h;
	const char *p;
	h = (void*)data;

	if (sizeof(struct ape_hdr_old) > len)
		return 0;

	uint flags = ffint_le_cpu16_ptr(h->flags);

	p = data + sizeof(struct ape_hdr_old);

	uint sz = sizeof(struct ape_hdr_old);
	if (flags & APE_FPEAK)
		sz += sizeof(int);
	if (flags & APE_FNSEEKEL)
		sz += sizeof(int);
	if (sz > len)
		return 0;

	uint comp_level = ffint_le_cpu16_ptr(h->comp_level);
	if (!(comp_level % 1000) && comp_level <= 5000)
		info->comp_level = comp_level / 1000;
	info->comp_level_orig = comp_level;

	uint frame_blocks;
	if (info->version >= 3950)
		frame_blocks = 73728 * 4;
	else if (info->version >= 3900 || (info->version >= 3800 && comp_level == 4000))
		frame_blocks = 73728;
	else
		frame_blocks = 9216;
	info->frame_blocks = frame_blocks;

	uint total_frames = ffint_le_cpu32_ptr(h->total_frames);
	uint lastframe_blocks = ffint_le_cpu32_ptr(h->lastframe_blocks);
	if (total_frames != 0 && lastframe_blocks < frame_blocks)
		info->total_samples = (total_frames - 1) * frame_blocks + lastframe_blocks;

	if (flags & APE_FPEAK)
		p += 4;

	if (flags & APE_FNSEEKEL) {
		uint seekpts = ffint_le_cpu32_ptr(p);
		p += 4;
		info->seekpoints = seekpts;
	} else
		info->seekpoints = total_frames;

	info->fmt.format = 16;
	if (flags & APE_F8BIT)
		info->fmt.format = 8;
	else if (flags & APE_F24BIT)
		info->fmt.format = 24;

	info->fmt.channels = ffint_le_cpu16_ptr(h->channels);
	info->fmt.rate = ffint_le_cpu32_ptr(h->rate);

	if (!(flags & APE_FNOWAVHDR)) {
		uint wavhdr_size = ffint_le_cpu32_ptr(h->wavhdr_size);
		if (sz + wavhdr_size > len)
			return 0;
		p += wavhdr_size;
	}

	return p - data;
}

/** Parse APE header.
Return bytes processed;  0 if more data is needed;  -APE_E* on error. */
static int ape_parse(ffape_info *info, const char *data, size_t len)
{
	const struct ape_desc *d;
	const struct ape_hdr *h;

	if (len < sizeof(8))
		return 0;

	d = (void*)data;
	if (!!ffmem_cmp(d->id, "MAC ", 4))
		return -APE_EHDR;
	info->version = ffint_le_cpu16_ptr(d->ver);

	if (info->version < 3980)
		return _ape_parse_old(info, data, len);

	uint desc_size = ffmax(sizeof(struct ape_desc), ffint_le_cpu32_ptr(d->desc_size));
	uint hdr_size = ffmax(sizeof(struct ape_hdr), ffint_le_cpu32_ptr(d->hdr_size));
	if ((uint64)desc_size + hdr_size > len)
		return 0;

	h = (void*)(data + desc_size);

	uint comp_level = ffint_le_cpu16_ptr(h->comp_level);
	if (!(comp_level % 1000) && comp_level <= 5000)
		info->comp_level = comp_level / 1000;
	info->comp_level_orig = comp_level;

	info->frame_blocks = ffint_le_cpu32_ptr(h->frame_blocks);

	uint total_frames = ffint_le_cpu32_ptr(h->total_frames);
	uint lastframe_blocks = ffint_le_cpu32_ptr(h->lastframe_blocks);
	if (total_frames != 0 && lastframe_blocks < info->frame_blocks)
		info->total_samples = (total_frames - 1) * info->frame_blocks + lastframe_blocks;

	uint bps = ffint_le_cpu16_ptr(h->bps);
	if (bps > 32)
		return -APE_EFMT;
	info->fmt.format = bps;

	info->fmt.channels = ffint_le_cpu16_ptr(h->channels);
	info->fmt.rate = ffint_le_cpu32_ptr(h->rate);

	uint seektbl_size = ffint_le_cpu32_ptr(d->seektbl_size);
	info->seekpoints = seektbl_size / 4;

	ffmem_copy(info->md5, d->md5, sizeof(d->md5));

	uint flags = ffint_le_cpu16_ptr(h->flags);
	if (!(flags & APE_FNOWAVHDR))
		info->wavhdr_size = ffint_le_cpu32_ptr(d->wavhdr_size);

	return desc_size + hdr_size;
}

static int _ffape_init(ffape *a)
{
	int r;
	struct ape_info info;
	info.version = a->info.version;
	info.compressionlevel = a->info.comp_level_orig;
	info.bitspersample = a->info.fmt.format;
	info.samplerate = a->info.fmt.rate;
	info.channels = a->info.fmt.channels;
	if (0 != (r = ape_decode_init(&a->ap, &info))) {
		a->err = r;
		return FFAPE_RERR;
	}

	if (NULL == (a->pcm = ffmem_alloc(a->info.frame_blocks * pcm_size1(&a->info.fmt)))) {
		a->err = APE_ESYS;
		return FFAPE_RERR;
	}

	return 0;
}

/**
Return enum FFAPE_R. */
int ffape_decode(ffape *a, ffstr *input, ffstr *output, ffuint block_start, ffuint block_samples, ffuint align4)
{
	int r;

	if (input->len == 0)
		return FFAPE_RMORE;

	if (!a->init) {
		r = ape_parse(&a->info, input->ptr, input->len);
		if (r == 0) {
			a->err = APE_ESMALL;
			return FFAPE_RERR;
		} else if (r < 0) {
			a->err = -r;
			return FFAPE_RERR;
		}
		if (0 != (r = _ffape_init(a)))
			return r;
		a->init = 1;
		input->len = 0;
		return FFAPE_RHDR;
	}

	r = ape_decode(a->ap, input->ptr, input->len, (void*)a->pcm, block_samples, align4);
	if (r < 0) {
		a->err = r;
		return FFAPE_RERR;
	}

	ffuint pcmlen = r;
	a->cursample = block_start;
	void *o = a->pcm;
	if (a->seek_sample != 0) {
		if (a->cursample < a->seek_sample && a->seek_sample < a->cursample + pcmlen) {
			uint n = a->seek_sample - a->cursample;
			a->cursample += n;
			o = (char*)a->pcm + n * pcm_size1(&a->info.fmt);
			pcmlen -= n;
		}
		a->seek_sample = 0;
	}

	a->block_samples = pcmlen;
	a->cursample += pcmlen;
	pcmlen *= pcm_size1(&a->info.fmt);
	ffstr_set(output, o, pcmlen);
	input->len = 0;
	return FFAPE_RDATA;
}
