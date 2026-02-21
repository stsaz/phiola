/** phiola: audio capture interface
2020, Simon Zolin */

#include <adev/audio.h>

typedef struct audio_in {
	// input
	const ffaudio_interface *audio;
	uint dev_idx; // 0:default
	phi_track *trk;
	const char *dev_id;
	ffuint buffer_length_msec; // output
	uint loopback;
	uint aflags;
	uint recv_events :1;

	// runtime
	ffaudio_buf *stream;
	uint64 total_samples;
	uint frame_size;
	uint state; // enum ST

#ifdef FF_WIN
	HANDLE event_h;
#endif
} audio_in;

static void audio_oncapt(void *udata);

/** Return FFAUDIO_E* */
static int audio_in_open(audio_in *a, phi_track *t)
{
	int rc = FFAUDIO_ERROR, r;
	ffbool first_try = 1;
	ffaudio_dev *dev = NULL;
	ffaudio_conf conf = {};

	FF_CAS(t->conf.iaudio.format.format, 0, PHI_PCM_16);
	FF_CAS(t->conf.iaudio.format.rate, 0, 44100);
	FF_CAS(t->conf.iaudio.format.channels, 0, 2);

	conf.device_id = a->dev_id;
	a->dev_idx = (t->conf.iaudio.device_id < 0xff) ? t->conf.iaudio.device_index : 0;
	if (a->dev_idx != 0) {
		ffuint mode = (a->loopback) ? FFAUDIO_DEV_PLAYBACK : FFAUDIO_DEV_CAPTURE;
		if (0 != audio_devbyidx(a->audio, &dev, a->dev_idx, mode)) {
			errlog(t, "no audio device by index #%u", a->dev_idx);
			goto err;
		}
		conf.device_id = a->audio->dev_info(dev, FFAUDIO_DEV_ID);
	}

	if (t->conf.iaudio.format.format != 0) {
		int afmt = phi_af_to_ffaudio(t->conf.iaudio.format.format);
		if (afmt < 0) {
			errlog(t, "format not supported", 0);
			goto err;
		}
		conf.format = afmt;
	}
	conf.sample_rate = t->conf.iaudio.format.rate;
	conf.channels = t->conf.iaudio.format.channels;
	conf.buffer_length_msec = (t->conf.iaudio.buf_time) ? t->conf.iaudio.buf_time : 500;

	ffaudio_conf in_conf = conf;

	int aflags = (a->loopback) ? FFAUDIO_LOOPBACK : FFAUDIO_CAPTURE;
	aflags |= a->aflags;

	if (a->recv_events) {
		conf.on_event = audio_oncapt;
		conf.udata = a;
	}

	if (NULL == (a->stream = a->audio->alloc())) {
		errlog(t, "create audio buffer");
		goto err;
	}

	for (;;) {
		dbglog(t, "opening device #%u, %s/%u/%u, flags:%xu"
			, a->dev_idx
			, ffaudio_format_str(conf.format), conf.sample_rate, conf.channels
			, aflags);
		r = a->audio->open(a->stream, &conf, aflags | FFAUDIO_O_NONBLOCK);

		if (r == FFAUDIO_EFORMAT) {
			if (first_try) {
				first_try = 0;
				struct phi_af f = {
					.format = ffaudio_to_phi_af(conf.format),
					.rate = conf.sample_rate,
					.channels = conf.channels,
				};
				t->conf.iaudio.format = f;
				continue;
			}

			if (aflags & FFAUDIO_O_HWDEV) {
				aflags &= ~FFAUDIO_O_HWDEV;
				continue;
			}

			errlog(t, "open device #%u: unsupported format: %s/%u/%u"
				, a->dev_idx
				, ffaudio_format_str(in_conf.format), in_conf.sample_rate, in_conf.channels);
			goto err;

		} else if (r != 0) {
			errlog(t, "open device #%u: %s  format:%s/%u/%u"
				, a->dev_idx
				, a->audio->error(a->stream)
				, ffaudio_format_str(in_conf.format), in_conf.sample_rate, in_conf.channels);
			rc = r;
			goto err;
		}

		break;
	}

	dbglog(t, "opened audio capture buffer: %s/%u/%u %ums"
		, ffaudio_format_str(conf.format), conf.sample_rate, conf.channels
		, conf.buffer_length_msec);

	a->buffer_length_msec = conf.buffer_length_msec;
	if (dev)
		a->audio->dev_free(dev);
	struct phi_af f = {
		.format = ffaudio_to_phi_af(conf.format),
		.rate = conf.sample_rate,
		.channels = conf.channels,
		.interleaved = 1,
	};
	t->audio.format = f;
	t->data_type = PHI_AC_PCM;
	a->frame_size = pcm_size1(&t->conf.iaudio.format);
	a->state = ST_SIGNALLED;

#ifdef FF_WIN
	a->event_h = conf.event_h;
#endif

	return 0;

err:
	if (dev)
		a->audio->dev_free(dev);
	a->audio->free(a->stream);
	a->stream = NULL;
	return rc;
}

static void audio_in_close(audio_in *a)
{
	a->audio->free(a->stream);
	a->stream = NULL;
}

static void audio_oncapt(void *udata)
{
	audio_in *a = udata;
	dbglog(a->trk, "%p state:%u", a, a->state);
	if (FF_SWAP(&a->state, ST_SIGNALLED) != ST_WAITING)
		return;
	core->track->wake(a->trk);
}

static int audio_in_read(audio_in *a, phi_track *t)
{
	int r;
	const void *buf;

	if (t->chain_flags & PHI_FSTOP) {
		a->audio->stop(a->stream);
		return PHI_DONE;
	}

#ifdef FF_WIN
	if (!!a->event_h && a->state == ST_SIGNALLED) {
		a->audio->signal(a->stream);
	}
#endif

	for (;;) {
		r = a->audio->read(a->stream, &buf);
		if (r == -FFAUDIO_ESYNC) {
			warnlog(t, "overrun detected");
			continue;

		} else if (r < 0) {
			ffstr extra = {};
			if (r == -FFAUDIO_EDEV_OFFLINE)
				ffstr_setz(&extra, "device disconnected: ");
			errlog(t, "audio device read: %S%s", &extra, a->audio->error(a->stream));
			t->error = PHI_E_AUDIO_INPUT;
			return PHI_DONE;

		} else if (r == 0) {
			a->state = ST_WAITING;
			return PHI_ASYNC;
		}
		break;
	}

	dbglog(t, "read %u bytes", r);

	t->audio.pos = a->total_samples;
	a->total_samples += r / a->frame_size;
	ffstr_set(&t->data_out, buf, r);
	a->state = ST_PROCESSING;
	return PHI_DATA;
}
