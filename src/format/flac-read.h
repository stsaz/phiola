/** phiola: .flac read
2021, Simon Zolin */

#include <avpack/flac-read.h>
#include <avpack/vorbistag.h>

struct flac_r {
	flacread fl;
	ffstr in;
	void *trk;
	uint sample_rate;
};

void flac_in_log(void *udata, const char *fmt, va_list va)
{
	struct flac_r *f = udata;
	phi_dbglogv(core, NULL, f->trk, fmt, va);
}

static void* flac_in_create(phi_track *t)
{
	struct flac_r *f = phi_track_allocT(t, struct flac_r);
	f->trk = t;

	ffuint64 size = (t->input.size != ~0ULL) ? t->input.size : 0;
	flacread_open(&f->fl, size);
	f->fl.log = flac_in_log;
	f->fl.udata = f;
	return f;
}

static void flac_in_free(void *ctx, phi_track *t)
{
	struct flac_r *f = ctx;
#ifdef FF_DEBUG
	dbglog(t, "data copied: %u", f->fl.stream.copied);
#endif
	flacread_close(&f->fl);
	phi_track_free(t, f);
}

static void flac_tags(struct flac_r *f, phi_track *t, ffstr vtag)
{
	vorbistagread vtr = {};
	for (;;) {
		ffstr name, val;
		int tag = vorbistagread_process(&vtr, &vtag, &name, &val);
		switch (tag) {
		case VORBISTAGREAD_DONE:
			return;
		case VORBISTAGREAD_ERROR:
			errlog(t, "bad Vorbis tags");
			return;
		}

		dbglog(t, "tags: %S: %S", &name, &val);
		if (tag > 0)
			ffstr_setz(&name, ffmmtag_str[tag]);
		core->metaif->set(&t->meta, name, val, 0);
	}
}

static void flac_info(struct flac_r *f, phi_track *t, const struct flac_info *i, int done)
{
	if (done) {
		dbglog(t, "blocksize:%u..%u  framesize:%u..%u  MD5:%16xb  seek-table:%u  meta-length:%u  total-samples:%,U"
			, (int)i->minblock, (int)i->maxblock, (int)i->minframe, (int)i->maxframe
			, i->md5, (int)f->fl.sktab.len, (int)f->fl.frame1_off, i->total_samples);
		t->audio.bitrate = i->bitrate;
		t->audio.flac_minblock = i->minblock;
		t->audio.flac_maxblock = i->maxblock;
		return;
	}

	t->audio.decoder = "FLAC";
	struct phi_af af = {
		.format = i->bits,
		.channels = i->channels,
		.rate = i->sample_rate,
	};
	t->audio.format = af;
	t->audio.format.interleaved = 0;
	t->data_type = "flac";
	t->audio.total = i->total_samples;
}

static int flac_in_read(void *ctx, phi_track *t)
{
	struct flac_r *f = ctx;
	int r;
	ffstr out = {};

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		f->in = t->data_in;
		t->data_in.len = 0;
		if (t->chain_flags & PHI_FFIRST)
			flacread_finish(&f->fl);
	}

	for (;;) {

		if (t->audio.seek_req && t->audio.seek != -1 && f->sample_rate != 0) {
			t->audio.seek_req = 0;
			flacread_seek(&f->fl, msec_to_samples(t->audio.seek, f->sample_rate));
			dbglog(t, "seek: %Ums", t->audio.seek);
		}

		r = flacread_process(&f->fl, &f->in, &out);
		switch (r) {
		case FLACREAD_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				warnlog(t, "file is incomplete");
				return PHI_DONE;
			}
			return PHI_MORE;

		case FLACREAD_HEADER:
			flac_info(f, t, flacread_info(&f->fl), 0);
			f->sample_rate = t->audio.format.rate;
			break;

		case FLACREAD_META_BLOCK:
			if (flacread_meta_type(&f->fl) == FLAC_TTAGS)
				flac_tags(f, t, out);
			break;

		case FLACREAD_HEADER_FIN:
			flac_info(f, t, flacread_info(&f->fl), 1);

			if (t->conf.info_only)
				return PHI_LASTOUT;

			if (!core->track->filter(t, core->mod("ac-flac.decode"), 0))
				return PHI_ERR;
			break;

		case FLACREAD_DATA:
			goto data;

		case FLACREAD_SEEK:
			t->input.seek = flacread_offset(&f->fl);
			return PHI_MORE;

		case FLACREAD_DONE:
			return PHI_DONE;

		case FLACREAD_ERROR:
			errlog(t, "flacread_decode(): at offset 0x%xU: %s"
				, flacread_offset(&f->fl), flacread_error(&f->fl));
			return PHI_ERR;
		}
	}

data:
	t->audio.pos = flacread_cursample(&f->fl);
	t->audio.flac_samples = flacread_samples(&f->fl);
	dbglog(t, "frame samples:%u @%U"
		, (int)t->audio.flac_samples, t->audio.pos);
	t->data_out = out;
	return PHI_DATA;
}

const phi_filter phi_flac_read = {
	flac_in_create, (void*)flac_in_free, (void*)flac_in_read,
	"flac-read"
};
