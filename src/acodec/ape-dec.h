/** phiola: APE input
2015, Simon Zolin */

#include <avpack/base/ape.h>
#include <MAC/MAC-phi.h>

struct ape_dec {
	struct ape_info info;
	struct ape_decoder *dec;
	void *pcm;
	uint sample_size;
	uint init;
};

static void* ape_dec_create(phi_track *t)
{
	if (!core->track->filter(t, core->mod("afilter.skip"), 0))
		return PHI_OPEN_ERR;

	struct ape_dec *a = phi_track_allocT(t, struct ape_dec);
	return a;
}

static void ape_dec_free(void *ctx, phi_track *t)
{
	struct ape_dec *a = ctx;
	if (a->dec)
		ape_decode_free(a->dec);
	phi_track_free(t, a->pcm);
	phi_track_free(t, a);
}

static void ape_info(struct ape_dec *a, phi_track *t, const struct ape_info *info)
{
	t->audio.decoder = "APE";
	struct phi_af f = {
		.format = info->bits,
		.channels = info->channels,
		.rate = info->sample_rate,
		.interleaved = 1,
	};
	t->audio.format = f;
	t->data_type = PHI_AC_PCM;
	t->audio.bitrate = bitrate_compute(t->input.size, t->audio.total, info->sample_rate);
	t->audio.total = info->total_samples;
}

static int ape_dec_init(struct ape_dec *a, phi_track *t)
{
	int r = ape_hdr_read(&a->info, t->data_in.ptr, t->data_in.len);
	if (r <= 0) {
		errlog(t, "bad APE header");
		return PHI_ERR;
	}

	switch (a->info.bits) {
	case 8:
	case 16:
	case 24:
	case 32:
		break;
	default:
		errlog(t, "bps value is not supported");
		return PHI_ERR;
	}

	struct ape_conf conf = {
		.version = a->info.version,
		.compressionlevel = a->info.comp_level,
		.bitspersample = a->info.bits,
		.samplerate = a->info.sample_rate,
		.channels = a->info.channels,
	};
	if ((r = ape_decode_init(&a->dec, &conf))) {
		errlog(t, "ape_decode_init(): %s", ape_errstr(r));
		return PHI_ERR;
	}

	a->sample_size = a->info.bits/8 * a->info.channels;
	a->pcm = phi_track_alloc(t, a->info.block_samples * a->sample_size);
	ape_info(a, t, &a->info);
	return 0;
}

static int ape_dec_decode(void *ctx, phi_track *t)
{
	struct ape_dec *a = ctx;
	int r;

	if (t->data_in.len == 0)
		return !(t->chain_flags & PHI_FFIRST) ? PHI_MORE : PHI_DONE;

	if (!a->init) {
		a->init = 1;
		if (ape_dec_init(a, t))
			return PHI_ERR;
		return PHI_DATA;
	}

	r = ape_decode(a->dec, t->data_in.ptr, t->data_in.len, (void*)a->pcm, t->audio.ape_block_samples, t->audio.ape_align4);
	if (r < 0) {
		errlog(t, "ape_decode(): %s", ape_errstr(r));
		return PHI_ERR;
	}

	ffstr_set(&t->data_out, a->pcm, r * a->sample_size);
	dbglog(t, "decoded %L samples @%U"
		, t->data_out.len / a->sample_size, t->audio.pos);
	return PHI_DATA;
}

static const phi_filter phi_ape_dec = {
	ape_dec_create, ape_dec_free, ape_dec_decode,
	"ape-decode"
};
