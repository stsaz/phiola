/** phiola/Android: conversion functionality
2023, Simon Zolin */

/** Parse seek/until audio position string: [[h:]m:]s[.ms] */
static int msec_apos(const char *apos, int64 *msec)
{
	ffstr s = FFSTR_INITZ(apos);
	if (s.len == 0)
		return 0;

	ffdatetime dt;
	if (s.len != fftime_fromstr1(&dt, s.ptr, s.len, FFTIME_HMS_MSEC_VAR))
		return -1;

	fftime t;
	fftime_join1(&t, &dt);
	*msec = fftime_to_msec(&t);
	return 0;
}

/** Get error message from PHI_E value */
static const char* trk_errstr(uint e)
{
	if (e == 0) return NULL;

	if (e & PHI_E_SYS)
		return fferr_strptr(e & ~PHI_E_SYS);

	e--;
	// enum PHI_E
	static const char errstr[][30] = {
		"Input file doesn't exist", // PHI_E_NOSRC
		"Output file already exists", // PHI_E_DSTEXIST
		"Unknown input file format", // PHI_E_UNKIFMT
		"Input audio device problem", // PHI_E_AUDIO_INPUT
		"Cancelled", // PHI_E_CANCELLED
		"Sample conversion", // PHI_E_ACONV
		"Output format is not supported", // PHI_E_OUT_FMT
	};
	const char *s = "Unknown";
	if (e < FF_COUNT(errstr))
		s = errstr[e];
	return s;
}


struct conv_track;
struct conv_track_info {
	union {
		struct {
			uint pos_sec, duration_sec;
			struct conv_track *ct;
			fflock lock;
			uint final;
			char *error;
		};
		char align[64];
	};
};

static void conv_grd_close(void *ctx, phi_track *t)
{
	uint i = x->queue.index(t->qent);
	struct conv_track_info *cti = (struct conv_track_info*)x->convert.tracks.ptr + i;

	if (t->chain_flags & PHI_FFINISHED) {
		if (t->error) {
			char *s = ffsz_dup(trk_errstr(t->error));
			ffcpu_fence_release(); // write data before pointer
			FFINT_WRITEONCE(cti->error, s);

		} else {
			const char *oname = (t->output.name) ? t->output.name : t->conf.ofile.name;

			if (x->convert.q_add_remove && x->convert.q_add) {
				struct phi_queue_entry qe = {
					.url = (char*)oname,
				};
				x->queue.add(x->convert.q_add_remove, &qe);
			}

			if (x->convert.trash_dir_rel && !ffsz_eq(t->conf.ifile.name, oname)) {
				char *trash_dir = trash_dir_abs(x->convert.trash_dir_rel, t->conf.ifile.name);
				if (trash_dir) {
					if (!file_trash(trash_dir, t->conf.ifile.name)
						&& x->convert.q_pos >= 0) {
						struct phi_queue_entry *qe = x->queue.ref(x->convert.q_add_remove, x->convert.q_pos);
						if (qe) {
							if (ffsz_eq(qe->url, t->conf.ifile.name))
								x->queue.remove(qe);
							x->queue.unref(qe);
						}
					}
				}
				ffmem_free(trash_dir);
			}
		}
	}

	cti->pos_sec = ~0U;
	x->core->track->stop(t);
}

static int conv_grd_process(void *ctx, phi_track *t)
{
	return PHI_DONE;
}

static const phi_filter phi_mconvert_guard = {
	NULL, conv_grd_close, conv_grd_process,
	"conv-guard"
};


struct conv_track {
	uint sample_rate;
	uint pos_sec, duration_sec;
	uint index;
};

static void* conv_ui_open(phi_track *t)
{
	struct conv_track *c = phi_track_allocT(t, struct conv_track);
	c->sample_rate = t->audio.format.rate;
	c->duration_sec = samples_to_msec(t->audio.total, c->sample_rate) / 1000;
	c->index = x->queue.index(t->qent);

	struct conv_track_info *cti = (struct conv_track_info*)x->convert.tracks.ptr + c->index;
	cti->duration_sec = c->duration_sec;
	cti->ct = c;
	ffcpu_fence_release(); // write data before counter
	ffint_fetch_add(&x->convert.tracks.len, 1); // multiple workers may increment the counter in parallel

	return c;
}

static void conv_ui_close(void *ctx, phi_track *t)
{
	struct conv_track *c = ctx;

	struct conv_track_info *cti = (struct conv_track_info*)x->convert.tracks.ptr + c->index;
	fflock_lock(&cti->lock); // all pending reads on `cti->ct` are complete
	cti->ct = NULL;
	fflock_unlock(&cti->lock);

	phi_track_free(t, c);
}

static int conv_ui_process(void *ctx, phi_track *t)
{
	struct conv_track *c = ctx;

	if (FFINT_READONCE(x->convert.interrupt)) {
		t->error = PHI_E_CANCELLED;
		return PHI_ERR;
	}

	if (t->audio.pos != ~0ULL)
		c->pos_sec = (uint)(samples_to_msec(t->audio.pos, c->sample_rate) / 1000);

	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

static const phi_filter phi_convert_ui = {
	conv_ui_open, conv_ui_close, conv_ui_process,
	"conv-ui"
};

enum {
	COF_DATE_PRESERVE = 1,
	COF_OVERWRITE = 2,
	COF_COPY = 4,
	COF_ADD = 8,
};
