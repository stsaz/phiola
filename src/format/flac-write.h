/** phiola: .flac writer
2018, Simon Zolin */

#include <track.h>
#include <avpack/flac-write.h>
#include <avpack/png-read.h>
#include <avpack/jpg-read.h>

typedef struct flac_w {
	flacwrite fl;
	ffstr in;
	uint state;
} flac_w;

static int pic_meta_png(struct flac_picinfo *info, const ffstr *data)
{
	pngread png = {};
	int rc = -1, r;
	pngread_open(&png);

	ffstr in = *data, out;
	r = pngread_process(&png, &in, &out);
	if (r != PNGREAD_HEADER)
		goto err;

	info->mime = "image/png";

	const struct png_info *i = pngread_info(&png);
	info->width = i->width;
	info->height = i->height;
	info->bpp = i->bpp;

	rc = 0;

err:
	pngread_close(&png);
	return rc;
}

static int pic_meta_jpeg(struct flac_picinfo *info, const ffstr *data)
{
	jpgread jpeg = {};
	int rc = -1, r;
	jpgread_open(&jpeg);

	ffstr in = *data, out;
	r = jpgread_process(&jpeg, &in, &out);
	if (r != JPGREAD_HEADER)
		goto err;

	info->mime = "image/jpeg";

	const struct jpg_info *i = jpgread_info(&jpeg);
	info->width = i->width;
	info->height = i->height;
	info->bpp = i->bpp;

	rc = 0;

err:
	jpgread_close(&jpeg);
	return rc;
}

static void pic_meta(struct flac_picinfo *info, const ffstr *data, void *trk)
{
	if (0 == pic_meta_png(info, data))
		return;
	if (0 == pic_meta_jpeg(info, data))
		return;
	warnlog(trk, "picture write: can't detect MIME; writing without MIME and image dimensions");
}

extern const phi_meta_if phi_metaif;
static int flac_out_addmeta(flac_w *f, phi_track *t)
{
	ffstr vendor = {};
	if (t->oaudio.flac_vendor != NULL)
		ffstr_setz(&vendor, t->oaudio.flac_vendor);
	if (0 != flacwrite_addtag(&f->fl, MMTAG_VENDOR, vendor)) {
		syserrlog(t, "can't add tag: vendor");
		return -1;
	}

	uint i = 0;
	ffstr name, val;
	while (phi_metaif.list(&t->meta, &i, &name, &val, PHI_META_UNIQUE)) {

		if (ffstr_eqcz(&name, "vendor"))
			continue;

		if (ffstr_eqcz(&name, "picture")) {
			struct flac_picinfo info = {};
			pic_meta(&info, &val, t);
			flacwrite_pic(&f->fl, &info, &val);
			continue;
		}

		if (0 != flacwrite_addtag_name(&f->fl, name, val)) {
			syserrlog(t, "can't add tag: %S", &name);
			return -1;
		}
	}
	return 0;
}

static void* flac_out_create(phi_track *t)
{
	flac_w *f = ffmem_new(flac_w);
	return f;
}

static void flac_out_free(void *ctx, phi_track *t)
{
	flac_w *f = ctx;
	flacwrite_close(&f->fl);
	ffmem_free(f);
}

static int flac_out_encode(void *ctx, phi_track *t)
{
	enum { I_FIRST, I_INIT, I_DATA0, I_DATA };
	flac_w *f = ctx;
	int r;

	switch (f->state) {
	case I_FIRST:
		if (!core->track->filter(t, core->mod("flac.encode"), PHI_TF_PREV))
			return PHI_ERR;
		f->state = I_INIT;
		return PHI_MORE;

	case I_INIT:
		if (!ffsz_eq(t->data_type, "flac")) {
			errlog(t, "unsupported input data format: %s", t->data_type);
			return PHI_ERR;
		}

		if (t->data_in.len != sizeof(struct flac_info)) {
			errlog(t, "invalid first input data block");
			return PHI_ERR;
		}

		ffuint64 total_samples = 0;
		if (t->audio.total != ~0ULL && !t->output.cant_seek)
			total_samples = (t->audio.total - t->audio.pos) * t->oaudio.format.rate / t->audio.format.rate;

		struct flac_info *info = (void*)t->data_in.ptr;

		flacwrite_create(&f->fl, info, total_samples);
		f->fl.seektable_interval = 1 * t->oaudio.format.rate;
		f->fl.min_meta = 1000;

		t->data_in.len = 0;
		if (0 != flac_out_addmeta(f, t))
			return PHI_ERR;

		f->state = I_DATA0;
		break;

	case I_DATA0:
	case I_DATA:
		break;
	}

	if (t->chain_flags & PHI_FFWD) {
		f->in = t->data_in;
		if (t->chain_flags & PHI_FFIRST) {
			if (t->data_in.len != sizeof(struct flac_info)) {
				errlog(t, "invalid last input data block");
				return PHI_ERR;
			}
			flacwrite_finish(&f->fl, (void*)t->data_in.ptr);
		}
	}

	ffstr out = {};

	for (;;) {
		r = flacwrite_process(&f->fl, &f->in, t->oaudio.flac_frame_samples, &out);

		switch (r) {
		case FLACWRITE_MORE:
			return PHI_MORE;

		case FLACWRITE_DATA:
			if (f->state == I_DATA0) {
				f->state = I_DATA;
			}
			goto data;

		case FLACWRITE_DONE:
			goto data;

		case FLACWRITE_SEEK:
			t->output.seek = flacwrite_offset(&f->fl);
			continue;

		case FLACWRITE_ERROR:
		default:
			errlog(t, "flacwrite_process(): %s", flacwrite_error(&f->fl));
			return PHI_ERR;
		}
	}

data:
	dbglog(t, "output: %L bytes", out.len);
	t->data_out = out;
	if (r == FLACWRITE_DONE)
		return PHI_DONE;
	return PHI_DATA;
}

const phi_filter phi_flac_write = {
	flac_out_create, (void*)flac_out_free, (void*)flac_out_encode,
	"flac-write"
};
