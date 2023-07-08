/** phiola: AAC ADTS (.aac) reader
2017, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <avpack/aac-read.h>

#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

extern const phi_core *core;

#include <format/aac-write.h>

struct aac_adts_r {
	aacread adts;
	uint64 pos;
	int sample_rate;
	int frno;
	ffstr in;
};

static void* aac_adts_open(phi_track *t)
{
	struct aac_adts_r *a = ffmem_new(struct aac_adts_r);
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
	ffmem_free(a);
}

static int aac_adts_process(void *ctx, phi_track *t)
{
	struct aac_adts_r *a = ctx;
	int r;

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

		case AACREAD_HEADER: {
			const struct aacread_info *info = aacread_info(&a->adts);
			struct phi_af f = {
				.format = PHI_PCM_16,
				.rate = info->sample_rate,
				.channels = info->channels,
			};
			t->audio.format = f;
			a->sample_rate = info->sample_rate;
			t->audio.total = 0;
			t->audio.decoder = "AAC";
			t->data_type = "aac";
			t->oaudio.mp4_frame_samples = 1024;

			if (!t->conf.stream_copy) {
				if (!core->track->filter(t, core->mod("aac.decode"), 0))
					return PHI_ERR;
			}

			t->data_out = out;
			return PHI_DATA;
		}

		case AACREAD_DATA:
		case AACREAD_FRAME:
			a->pos += aacread_frame_samples(&a->adts);
			if (t->audio.seek_req && t->audio.seek != -1) {
				uint64 seek_samps = msec_to_samples(t->audio.seek, a->sample_rate);
				dbglog(t, "seek: tgt:%U  @%U", seek_samps, a->pos);
				if (a->pos < seek_samps)
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
	t->audio.pos = a->pos;
	dbglog(t, "passing frame #%u  samples:%u @%U  size:%u"
		, a->frno++, aacread_frame_samples(&a->adts), t->audio.pos
		, out.len);
	t->data_out = out;
	return PHI_DATA;
}

const phi_filter phi_aac_adts_read = {
	aac_adts_open, (void*)aac_adts_close, (void*)aac_adts_process,
	"aac-adts-read"
};
