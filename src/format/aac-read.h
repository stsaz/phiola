/** phiola: .aac reader
2017, Simon Zolin */

#include <avpack/aac-read.h>
#include <util/util.h>

struct aac_adts_r {
	aacread adts;
	uint64 pos;
	int sample_rate;
	int frno;
	ffstr in;
};

static void* aac_adts_open(phi_track *t)
{
	struct aac_adts_r *a = phi_track_allocT(t, struct aac_adts_r);
	if (t->conf.stream_copy) {
		ffstr fn = FFSTR_INITZ(t->conf.ofile.name), ext;
		ffstr_rsplitby(&fn, '.', NULL, &ext);
		if (ffstr_ieqz(&ext, "aac")) // return the whole adts frames only if the output is .aac file
			a->adts.options = AACREAD_WHOLEFRAME;
	}
	aacread_open(&a->adts);
	return a;
}

static void aac_adts_close(void *ctx, phi_track *t)
{
	struct aac_adts_r *a = ctx;
	aacread_close(&a->adts);
	phi_track_free(t, a);
}

static void aac_info(struct aac_adts_r *a, phi_track *t, const struct aacread_info *info)
{
	struct phi_af f = {
		.format = PHI_PCM_16,
		.rate = info->sample_rate,
		.channels = info->channels,
	};
	t->audio.format = f;
	t->audio.total = 0;
	t->audio.decoder = "AAC";
	t->data_type = "aac";
	t->oaudio.mp4_frame_samples = 1024;
}

static int aac_adts_process(void *ctx, phi_track *t)
{
	struct aac_adts_r *a = ctx;
	int r;
	uint64 apos = 0;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		a->in = t->data_in;
	}

	ffstr out = {};

	for (;;) {
		r = aacread_process(&a->adts, &a->in, &out);

		switch (r) {

		case AACREAD_HEADER:
			aac_info(a, t, aacread_info(&a->adts));
			a->sample_rate = t->audio.format.rate;

			if (!t->conf.stream_copy) {
				if (!core->track->filter(t, core->mod("ac-aac.decode"), 0))
					return PHI_ERR;
			}

			t->data_out = out;
			return PHI_DATA;

		case AACREAD_DATA:
		case AACREAD_FRAME:
			apos = a->pos;
			a->pos += aacread_frame_samples(&a->adts);
			if (t->audio.seek_req && t->audio.seek != -1) {
				uint64 seek_samps = msec_to_samples(t->audio.seek, a->sample_rate);
				dbglog(t, "seek: tgt:%U  @%U", seek_samps, apos);
				if (apos < seek_samps)
					continue;
				t->audio.seek_req = 0;
			}
			goto data;

		case AACREAD_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				return PHI_LASTOUT;
			}
			return PHI_MORE;

		case AACREAD_WARN:
			warnlog(t, "aacread_process(): %s.  Offset: %U"
				, aacread_error(&a->adts), aacread_offset(&a->adts));
			continue;

		case AACREAD_ERROR:
			errlog(t, "aacread_process(): %s.  Offset: %U"
				, aacread_error(&a->adts), aacread_offset(&a->adts));
			return PHI_ERR;

		default:
			FF_ASSERT(0);
			return PHI_ERR;
		}
	}

data:
	if (t->conf.info_only && a->frno > 32) { // # of frames to detect real AAC format
		return PHI_LASTOUT;
	}

	t->audio.pos = apos;
	dbglog(t, "passing frame #%u  samples:%u @%U  size:%u"
		, a->frno, aacread_frame_samples(&a->adts), t->audio.pos
		, out.len);
	a->frno++;
	t->data_out = out;
	return PHI_DATA;
}

const phi_filter phi_aac_adts_read = {
	aac_adts_open, (void*)aac_adts_close, (void*)aac_adts_process,
	"aac-adts-read"
};
