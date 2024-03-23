/** phiola: audio playback interface
2020, Simon Zolin */

#include <adev/audio.h>

#define ABUF_CLOSE_WAIT  3000

typedef struct audio_out audio_out;
struct audio_out {
	// input
	const ffaudio_interface *audio;
	uint buffer_length_msec; // input, output
	uint try_open;
	uint dev_idx; // 0:default
	phi_track *trk;
	uint aflags;
	int err_code; // enum FFAUDIO_E
	int handle_dev_offline;

	// runtime
	ffaudio_buf *stream;
	ffaudio_dev *dev;
	uint state; // enum ST
	uint clear :1;

	// user's
	uint reconnect :1;
#ifdef FF_WIN
	struct phi_woeh_task task;
	HANDLE event_h;
#endif
};

/**
Return FFAUDIO_E*
Return FFAUDIO_EFORMAT (if try_open==1): requesting audio conversion */
static int audio_out_open(audio_out *a, phi_track *t, const struct phi_af *fmt)
{
	if (!ffsz_eq(t->data_type, "pcm")) {
		errlog(t, "input data type not supported: %s", t->data_type);
		return FFAUDIO_ERROR;
	}

	int rc = FFAUDIO_ERROR, r;
	ffaudio_conf conf = {};

	a->dev_idx = t->conf.oaudio.device_index;
	if (a->dev == NULL
		&& a->dev_idx != 0) {
		if (0 != audio_devbyidx(a->audio, &a->dev, a->dev_idx, FFAUDIO_DEV_PLAYBACK)) {
			errlog(t, "no audio device by index #%u", a->dev_idx);
			goto end;
		}
	}

	if (a->dev != NULL)
		conf.device_id = a->audio->dev_info(a->dev, FFAUDIO_DEV_ID);

	if (NULL == (a->stream = a->audio->alloc())) {
		errlog(t, "audio buffer alloc");
		goto end;
	}

	int afmt = phi_af_to_ffaudio(fmt->format);
	if (afmt < 0) {
		errlog(t, "format not supported", 0);
		goto end;
	}
	conf.format = afmt;
	conf.sample_rate = fmt->rate;
	conf.channels = fmt->channels;
	conf.buffer_length_msec = (t->conf.oaudio.buf_time) ? t->conf.oaudio.buf_time : 500;

	uint aflags = a->aflags;
	ffaudio_conf in_conf = conf;
	dbglog(t, "opening device #%u, %s/%u/%u, flags:%xu"
		, a->dev_idx
		, ffaudio_format_str(conf.format), conf.sample_rate, conf.channels
		, aflags);
	r = a->audio->open(a->stream, &conf, FFAUDIO_PLAYBACK | FFAUDIO_O_NONBLOCK | aflags);

	if (r == FFAUDIO_EFORMAT) {
		if (a->try_open) {
			int new_format = 0;
			if (conf.format != in_conf.format) {
				t->oaudio.conv_format.format = ffaudio_to_phi_af(conf.format);
				new_format = 1;
			}

			if (conf.sample_rate != in_conf.sample_rate) {
				t->oaudio.conv_format.rate = conf.sample_rate;
				new_format = 1;
			}

			if (conf.channels != in_conf.channels) {
				t->oaudio.conv_format.channels = conf.channels;
				new_format = 1;
			}

			if (new_format) {
				rc = FFAUDIO_EFORMAT;
				goto end;
			}
		}

		errlog(t, "open(): format not supported: %s/%u/%u"
			, ffaudio_format_str(conf.format), conf.sample_rate, conf.channels);
		goto end;

	} else if (r == FFAUDIO_ECONNECTION) {
		warnlog(t, "lost connection to audio server: %s", a->audio->error(a->stream));
		rc = FFAUDIO_ECONNECTION;
		goto end;

	} else if (r != 0) {
		errlog(t, "open() device #%u: %s  format:%s/%u/%u"
			, a->dev_idx
			, a->audio->error(a->stream)
			, ffaudio_format_str(conf.format), conf.sample_rate, conf.channels);
		rc = FFAUDIO_ERROR;
		goto end;
	}
	a->buffer_length_msec = conf.buffer_length_msec;
	a->state = ST_FEEDING;

#ifdef FF_WIN
	a->event_h = conf.event_h;
#endif

	rc = 0;

end:
	if (rc != 0) {
		a->audio->free(a->stream);
		a->stream = NULL;
	}
	return rc;
}

static void audio_out_onplay(void *param)
{
	audio_out *a = param;
	dbglog(a->trk, "%p state:%u", a, a->state);
	if (FF_SWAP(&a->state, ST_SIGNALLED) != ST_WAITING)
		return;
	core->track->wake(a->trk);
}

static inline void audio_out_stop(audio_out *a)
{
	a->trk->chain_flags |= PHI_FSTOP;
	audio_out_onplay(a);
}

static int audio_out_write(audio_out *a, phi_track *t)
{
	int r;

	if (t->chain_flags & PHI_FSTOP)
		return PHI_FIN;

	if (t->oaudio.clear) {
		t->oaudio.clear = 0;
		dbglog(t, "stop/clear");
		if (0 != a->audio->stop(a->stream))
			warnlog(a->trk, "audio.stop: %s", a->audio->error(a->stream));
		if (0 != a->audio->clear(a->stream))
			warnlog(t, "audio.clear: %s", a->audio->error(a->stream));
		if (t->audio.seek_req)
			return PHI_MORE;
	}

	if (t->oaudio.pause) {
		t->oaudio.pause = 0;
		if (0 != a->audio->stop(a->stream))
			warnlog(t, "pause: audio.stop: %s", a->audio->error(a->stream));
		return PHI_ASYNC;
	}

#ifdef FF_WIN
	if (!!a->event_h && a->state == ST_SIGNALLED) {
		a->audio->signal(a->stream);
	}
#endif

	while (t->data_in.len != 0) {

		r = a->audio->write(a->stream, t->data_in.ptr, t->data_in.len);
		if (r > 0) {
			//
		} else if (r == 0) {
			a->state = ST_WAITING;
			return PHI_ASYNC;

		} else if (r == -FFAUDIO_ESYNC) {
			warnlog(t, "underrun detected", 0);
			continue;

		} else if (r == -FFAUDIO_EDEV_OFFLINE && a->handle_dev_offline) {
			warnlog(t, "audio device write: device disconnected: %s", a->audio->error(a->stream));
			a->err_code = FFAUDIO_EDEV_OFFLINE;
			return PHI_ERR;

		} else {
			ffstr extra = {};
			if (r == -FFAUDIO_EDEV_OFFLINE)
				ffstr_setz(&extra, "device disconnected: ");
			errlog(t, "audio device write: %S%s", &extra, a->audio->error(a->stream));
			a->err_code = -r;
			return PHI_ERR;
		}

		ffstr_shift(&t->data_in, r);
		dbglog(t, "written %u bytes", r);
	}

	if (t->chain_flags & PHI_FFIRST) {

		r = a->audio->drain(a->stream);
		if (r == 1)
			return PHI_DONE;
		else if (r < 0) {
			errlog(t, "drain(): %s", a->audio->error(a->stream));
			a->err_code = FFAUDIO_ERROR;
			return PHI_ERR;
		}

		a->state = ST_WAITING;
		return PHI_ASYNC; //wait until all filled bytes are played
	}

	a->state = ST_FEEDING;
	return PHI_MORE;
}

static void audio_stop(void *ctx)
{
	audio_out *a = ctx;
	dbglog(a->trk, "stop");
	if (a->audio->stop(a->stream))
		warnlog(a->trk, "audio.stop: %s", a->audio->error(a->stream));
	audio_out_onplay(a);
}
