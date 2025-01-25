/** phiola: FLAC-OGG input.
2019, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>
#include <avpack/flac-ogg-read.h>

#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)
extern const phi_core *core;

struct flacogg_r {
	flacoggread fo;
	uint64 apos, page_start_pos;
	ffstr in;
	uint fr_samples;
};

static void* flacogg_in_create(phi_track *t)
{
	struct flacogg_r *f = phi_track_allocT(t, struct flacogg_r);
	flacoggread_open(&f->fo);
	return f;
}

static void flacogg_in_free(void *ctx, phi_track *t)
{
	struct flacogg_r *f = ctx;
	flacoggread_close(&f->fo);
	phi_track_free(t, f);
}

static void flacogg_meta(struct flacogg_r *f, phi_track *t)
{
	ffstr name, val;
	int tag = flacoggread_tag(&f->fo, &name, &val);
	if (tag != 0)
		ffstr_setz(&name, ffmmtag_str[tag]);
	dbglog(t, "%S: %S", &name, &val);
	core->metaif->set(&t->meta, name, val, 0);
}

static int flacogg_info(struct flacogg_r *f, phi_track *t, const struct flac_info *info, int done)
{
	if (done) {
		if (info->minblock != info->maxblock) {
			errlog(t, "unsupported case: minblock != maxblock");
			return -1;
		}

		t->audio.flac_minblock = info->minblock;
		t->audio.flac_maxblock = info->maxblock;
		return 0;
	}

	t->audio.decoder = "FLAC";
	struct phi_af af = {
		.format = info->bits,
		.channels = info->channels,
		.rate = info->sample_rate,
	};
	t->audio.format = af;
	t->data_type = "flac";
	return 0;
}

static int flacogg_in_read(void *ctx, phi_track *t)
{
	struct flacogg_r *f = ctx;
	int r;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		f->in = t->data_in;
		t->data_in.len = 0;
		if (f->page_start_pos != t->audio.pos) {
			f->apos = t->audio.pos;
			f->page_start_pos = t->audio.pos;
		}
	}

	ffstr out = {};

	for (;;) {
		r = flacoggread_process(&f->fo, &f->in, &out);

		switch (r) {
		case FLACOGGREAD_HEADER:
			flacogg_info(f, t, flacoggread_info(&f->fo), 0);
			break;

		case FLACOGGREAD_TAG:
			flacogg_meta(f, t);
			break;

		case FLACOGGREAD_HEADER_FIN:
			if (t->conf.info_only)
				return PHI_LASTOUT;

			if (flacogg_info(f, t, flacoggread_info(&f->fo), 1))
				return PHI_ERR;

			f->fr_samples = t->audio.flac_minblock;

			if (!core->track->filter(t, core->mod("ac-flac.decode"), 0))
				return PHI_ERR;
			break;

		case FLACOGGREAD_DATA:
			goto data;

		case FLACOGGREAD_MORE:
			if (t->chain_flags & PHI_FFIRST)
				return PHI_DONE;
			return PHI_MORE;

		case FLACOGGREAD_ERROR:
			errlog(t, "flacoggread_read(): %s", flacoggread_error(&f->fo));
			return PHI_ERR;

		default:
			errlog(t, "flacoggread_read(): %r", r);
			return PHI_ERR;
		}
	}

data:
	dbglog(t, "frame size:%L  @%U", out.len, f->apos);
	t->audio.pos = f->apos;
	t->audio.flac_samples = f->fr_samples;
	f->apos += f->fr_samples;
	t->data_out = out;
	return PHI_DATA;
}

const phi_filter phi_flacogg_read = {
	flacogg_in_create, (void*)flacogg_in_free, (void*)flacogg_in_read,
	"flac-ogg-read"
};
