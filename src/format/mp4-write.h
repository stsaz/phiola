/** phiola: .mp4 write
2021, Simon Zolin */

#include <avpack/mp4-write.h>

extern const phi_meta_if phi_metaif;

struct mp4_w {
	uint state;
	mp4write mp;
	ffstr in;
	uint stream_copy :1;
};

static void* mp4w_create(phi_track *t)
{
	struct mp4_w *m = ffmem_new(struct mp4_w);
	return m;
}

static void mp4w_free(struct mp4_w *m, phi_track *t)
{
	mp4write_close(&m->mp);
	ffmem_free(m);
}

static int mp4w_addmeta(struct mp4_w *m, phi_track *t)
{
	uint i = 0;
	ffstr name, val;
	while (phi_metaif.list(&t->meta, &i, &name, &val, PHI_META_UNIQUE)) {
		if (ffstr_eqcz(&name, "vendor"))
			continue;

		int tag;
		if (-1 == (tag = ffszarr_find(ffmmtag_str, FF_COUNT(ffmmtag_str), name.ptr, name.len))) {
			warnlog(t, "unsupported tag: %S", &name);
			continue;
		}

		if (0 != mp4write_addtag(&m->mp, tag, val)) {
			warnlog(t, "can't add tag: %S", &name);
		}
	}
	return 0;
}

/* Encoding process:
. Add encoder filter to the chain
. Get encoder config data
. Initialize MP4 output
. Wrap encoded audio data into MP4 */
static int mp4w_write(struct mp4_w *m, phi_track *t)
{
	int r;

	switch (m->state) {

	case 0:
		if (ffsz_eq(t->data_type, "aac")) {
			m->state = 1;

		} else if (ffsz_eq(t->data_type, "pcm")) {
			if (!core->track->filter(t, core->mod("aac.encode"), PHI_TF_PREV))
				return PHI_ERR;
			m->state = 1;
			return PHI_MORE;

		} else if (ffsz_eq(t->data_type, "mp4")) {
			m->state = 1;
			m->stream_copy = 1;

		} else {
			errlog(t, "input data format not supported: %s", t->data_type);
			return PHI_ERR;
		}
		// fallthrough

	case 1: {
		const struct phi_af *of = &t->oaudio.format;

		if (t->oaudio.mp4_frame_samples == 0
			|| of->format == PHI_PCM_FLOAT32
			|| of->format == PHI_PCM_FLOAT64) {
			errlog(t, "bad input");
			return PHI_ERR;
		}

		struct mp4_info info = {
			.conf = t->data_in,
			.fmt.bits = pcm_bits(of->format),
			.fmt.channels = of->channels,
			.fmt.rate = of->rate,
			.frame_samples = t->oaudio.mp4_frame_samples,
			.enc_delay = t->oaudio.mp4_delay,
			.bitrate = t->oaudio.mp4_bitrate,
		};
		if (0 != (r = mp4write_create_aac(&m->mp, &info))) {
			errlog(t, "ffmp4_create_aac(): %s", mp4write_error(&m->mp));
			return PHI_ERR;
		}

		if (0 != mp4w_addmeta(m, t))
			return PHI_ERR;

		t->data_in.len = 0;
		m->state = 2;
		break;
	}
	}

	if (t->chain_flags & PHI_FFWD) {
		m->in = t->data_in;
		if (t->chain_flags & PHI_FFIRST)
			m->mp.fin = 1;

	}

	for (;;) {
		r = mp4write_process(&m->mp, &m->in, &t->data_out);
		switch (r) {
		case MP4WRITE_SEEK:
			t->output.seek = m->mp.off;
			continue;

		case MP4WRITE_DATA:
			goto data;

		case MP4WRITE_DONE:
			verblog(t, "MP4: frames:%u, overhead: %.2F%%"
				, m->mp.frameno
				, (double)m->mp.mp4_size * 100 / (m->mp.mp4_size + m->mp.mdat_size));
			return PHI_DONE;

		case MP4WRITE_MORE:
			return PHI_MORE;

		case MP4WRITE_ERROR:
			errlog(t, "mp4write_process(): %s", mp4write_error(&m->mp));
			return PHI_ERR;
		}
	}

data:
	return PHI_DATA;
}

const struct phi_filter phi_mp4_write = {
	mp4w_create, (void*)mp4w_free, (void*)mp4w_write,
	"mp4-write"
};
