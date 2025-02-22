/** phiola: .wv reader
2021, Simon Zolin */

#include <track.h>
#include <util/util.h>
#include <format/mmtag.h>
#include <avpack/wv-read.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

typedef struct wvpk_r {
	wvread wv;
	phi_track *trk;
	ffstr in;
	uint sample_rate;
	uint hdr_done;
} wvpk_r;

static void wv_log(void *udata, const char *fmt, va_list va)
{
	wvpk_r *w = udata;
	phi_dbglogv(core, NULL, w->trk, fmt, va);
}

static void* wv_in_create(phi_track *t)
{
	wvpk_r *w = phi_track_allocT(t, wvpk_r);
	w->trk = t;
	ffuint64 fs = (t->input.size != ~0ULL) ? t->input.size : 0;
	wvread_open(&w->wv, fs);
	w->wv.log = wv_log;
	w->wv.udata = w;
	w->wv.id3v1.codepage = core->conf.code_page;
	return w;
}

static void wv_in_free(void *ctx, phi_track *t)
{
	wvpk_r *w = ctx;
	wvread_close(&w->wv);
	phi_track_free(t, w);
}

static void wv_in_meta(wvpk_r *w, phi_track *t)
{
	ffstr name, val;
	int tag = wvread_tag(&w->wv, &name, &val);
	if (tag != 0)
		ffstr_setz(&name, ffmmtag_str[tag]);
	dbglog(t, "tag: %S: %S", &name, &val);
	core->metaif->set(&t->meta, name, val, 0);
}

static int wv_in_process(void *ctx, phi_track *t)
{
	wvpk_r *w = ctx;
	int r;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		wvread_eof(&w->wv, t->data_in.len == 0);
		ffstr_setstr(&w->in, &t->data_in);
		t->data_in.len = 0;
	}

again:
	if (w->hdr_done && t->audio.seek_req && t->audio.seek != -1) {
		t->audio.seek_req = 0;
		wvread_seek(&w->wv, msec_to_samples(t->audio.seek, w->sample_rate));
		dbglog(t, "seek: %Ums", t->audio.seek);
	}

	for (;;) {
		r = wvread_process(&w->wv, &w->in, &t->data_out);
		switch (r) {
		case WVREAD_ID31:
		case WVREAD_APETAG:
			wv_in_meta(w, t);
			break;

		case WVREAD_DATA:
			if (!w->hdr_done) {
				w->hdr_done = 1;
				const struct wvread_info *info = wvread_info(&w->wv);
				t->audio.total = info->total_samples;

				struct phi_af f = {
					.format = info->bits,
					.channels = info->channels,
					.rate = info->sample_rate,
				};
				if (info->flt) {
					if (info->bits != 32) {
						errlog(t, "audio format is not supported");
						return PHI_ERR;
					}
					f.format = PHI_PCM_FLOAT32;
				}
				t->audio.format = f;

				t->audio.bitrate = bitrate_compute(t->input.size, info->total_samples, info->sample_rate);
				if (!core->track->filter(t, core->mod("ac-wavpack.decode"), 0))
					return PHI_ERR;
				w->sample_rate = info->sample_rate;
				if (t->audio.seek_req && t->audio.seek != -1)
					goto again;
			}
			goto data;

		case WVREAD_SEEK:
			t->input.seek = wvread_offset(&w->wv);
			return PHI_MORE;

		case WVREAD_MORE:
			if (t->chain_flags & PHI_FFIRST) {
				return PHI_LASTOUT;
			}
			return PHI_MORE;

		case WVREAD_WARN:
			warnlog(t, "wvread_read(): at offset %xU: %s"
				, wvread_offset(&w->wv), wvread_error(&w->wv));
			break;

		case WVREAD_ERROR:
			errlog(t, "wvread_read(): %s", wvread_error(&w->wv));
			return PHI_ERR;
		}
	}

data:
	dbglog(t, "frame: %L bytes (@%U)"
		, t->data_out.len, wvread_cursample(&w->wv));
	t->audio.pos = wvread_cursample(&w->wv);
	return PHI_DATA;
}

const phi_filter phi_wv_read = {
	wv_in_create, wv_in_free, wv_in_process,
	"wv-read"
};
