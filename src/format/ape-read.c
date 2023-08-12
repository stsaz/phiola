/** phiola: .ape reader
2021, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>
#include <avpack/ape-read.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct ape_r {
	aperead ape;
	ffstr in;
	uint state;
	uint sample_rate;
};

static void* ape_in_create(phi_track *t)
{
	struct ape_r *a = ffmem_new(struct ape_r);
	ffuint64 fs = (t->input.size != ~0ULL) ? t->input.size : 0;
	aperead_open(&a->ape, fs);
	a->ape.id3v1.codepage = core->conf.code_page;
	return a;
}

static void ape_in_free(void *ctx, phi_track *t)
{
	struct ape_r *a = ctx;
	aperead_close(&a->ape);
	ffmem_free(a);
}

extern const phi_meta_if phi_metaif;
static void ape_in_meta(struct ape_r *a, phi_track *t)
{
	ffstr name, val;
	int tag = aperead_tag(&a->ape, &name, &val);
	if (tag != 0)
		ffstr_setz(&name, ffmmtag_str[tag]);
	dbglog(t, "tag: %S: %S", &name, &val);
	phi_metaif.set(&t->meta, name, val, 0);
}

static int ape_in_process(void *ctx, phi_track *t)
{
	enum { I_HDR, I_HDR_PARSED, I_DATA };
	struct ape_r *a = ctx;
	int r;

	if (t->chain_flags & PHI_FSTOP) {

		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		ffstr_setstr(&a->in, &t->data_in);
		t->data_in.len = 0;
	}

	switch (a->state) {
	case I_HDR:
		break;

	case I_HDR_PARSED:
		a->sample_rate = t->audio.format.rate;
		a->state = I_DATA;
		// fallthrough

	case I_DATA:
		if (t->audio.seek_req && t->audio.seek != -1) {
			t->audio.seek_req = 0;
			aperead_seek(&a->ape, msec_to_samples(t->audio.seek, a->sample_rate));
			dbglog(t, "seek: %Ums", t->audio.seek);
		}
		break;
	}

	for (;;) {
		r = aperead_process(&a->ape, &a->in, &t->data_out);
		switch (r) {
		case APEREAD_ID31:
		case APEREAD_APETAG:
			ape_in_meta(a, t);
			break;

		case APEREAD_HEADER:
			if (!core->track->filter(t, core->mod("ape.decode"), 0))
				return PHI_ERR;
			a->state = I_HDR_PARSED;
			return PHI_DATA;

		case APEREAD_DONE:
		case APEREAD_DATA:
			goto data;

		case APEREAD_SEEK:
			t->input.seek = aperead_offset(&a->ape);
			return PHI_MORE;

		case APEREAD_MORE:
			if (t->chain_flags & PHI_FFIRST) {

				return PHI_LASTOUT;
			}
			return PHI_MORE;

		case APEREAD_WARN:
			warnlog(t, "aperead_read(): at offset %xU: %s"
				, aperead_offset(&a->ape), aperead_error(&a->ape));
			break;

		case APEREAD_ERROR:
			errlog(t, "aperead_read(): %s", aperead_error(&a->ape));
			return PHI_ERR;

		default:
			FF_ASSERT(0);
			return PHI_ERR;
		}
	}

data:
	dbglog(t, "frame: %L bytes (@%U)"
		, t->data_out.len, aperead_cursample(&a->ape));
	t->audio.pos = aperead_cursample(&a->ape);
	t->audio.ape_block_samples = aperead_block_samples(&a->ape);
	t->audio.ape_align4 = aperead_align4(&a->ape);
	return (r == APEREAD_DATA) ? PHI_DATA : PHI_LASTOUT;
}

const phi_filter phi_ape_read = {
	ape_in_create, (void*)ape_in_free, (void*)ape_in_process,
	"ape-read"
};
