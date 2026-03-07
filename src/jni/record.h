/** phiola/Android: recording functionality
2023, Simon Zolin */

struct RecordCallback {
	jobject obj;
	jmethodID on_finish;
};

struct rec_ctx {
	uint state;
	struct RecordCallback RecordCallback;
};

enum {
	RS_PAUSED = 1,
};


static void rectrk_close(void *ctx, phi_track *t)
{
	struct rec_ctx *rx = t->udata;
	JNIEnv *env;
	int r = jni_vm_attach(jvm, &env);
	if (r != 0) {
		errlog("jni_vm_attach: %d", r);
		goto end;
	}

	const char *fn = (t->output.name) ? t->output.name : t->conf.ofile.name;
	jstring joname = jni_js_sz(fn);
	jni_call_void(rx->RecordCallback.obj, rx->RecordCallback.on_finish, t->error, joname);

end:
	jni_global_unref(rx->RecordCallback.obj);
	ffmem_free(rx);
	jni_vm_detach(jvm);
}

static int rectrk_process(void *ctx, phi_track *t)
{
	return PHI_DONE;
}

static const phi_filter phi_android_guard = {
	NULL, rectrk_close, rectrk_process,
	"rec-guard"
};


static int rec_ctl_process(void *ctx, phi_track *t)
{
	struct rec_ctx *rx = t->udata;
	if (t->chain_flags & PHI_FFIRST)
		return PHI_DONE;
	if (!(t->chain_flags & PHI_FFWD)) {
		if (rx->state & RS_PAUSED)
			return PHI_ASYNC;
		return PHI_MORE;
	}
	t->data_out = t->data_in;
	return PHI_DATA;
}

static const phi_filter phi_android_rec_ctl = {
	NULL, NULL, rec_ctl_process,
	"rec-ctl"
};


#define RECF_EXCLUSIVE  1
#define RECF_POWER_SAVE  2
#define RECF_DANORM  8

struct RecordParams {
	jint format;
	jint channels;
	jint sample_rate;
	jint flags;
	jstring src_preset;
	jint buf_len_msec;
	jint gain_db100;
	jint quality;
	jint until_sec;
};

#define _I(name)  { #name, 'i', FF_OFF(struct RecordParams, name), 0 }
#define _S(name)  { #name, 's', FF_OFF(struct RecordParams, name), 0 }
static struct jni_cmap RecordParams_map[] = {
	_I(format),
	_I(channels),
	_I(sample_rate),
	_I(flags),
	_S(src_preset),
	_I(buf_len_msec),
	_I(gain_db100),
	_I(quality),
	_I(until_sec),
	{},
};
#undef _I
#undef _S

JNIEXPORT jlong JNICALL
Java_com_github_stsaz_phiola_Phiola_recStart(JNIEnv *env, jobject thiz, jstring joname, jobject jconf, jobject jcb)
{
	dbglog("%s: enter", __func__);
	int e = -1;
	struct RecordParams rp = {};
	jclass jc_conf = jni_class_obj(jconf);
	jni_obj_read(env, &rp, RecordParams_map, jconf, jc_conf);
	const char *oname = jni_sz_js(joname);

	const char *dev_id = jni_sz_js(rp.src_preset);
	ffmem_free(x->rec.device_id);
	x->rec.device_id = (*dev_id) ? ffsz_dup(dev_id) : NULL;

	struct phi_track_conf c = {
		.iaudio = {
			.format = {
				.channels = rp.channels,
				.rate = rp.sample_rate,
			},
			.device_id = (size_t)x->rec.device_id,
			.buf_time = rp.buf_len_msec,
			.exclusive = !!(rp.flags & RECF_EXCLUSIVE),
			.power_save = !!(rp.flags & RECF_POWER_SAVE),
		},
		.until_msec = rp.until_sec * 1000,
		.afilter = {
			.gain_db = (double)rp.gain_db100 / 100,
			.danorm = (rp.flags & RECF_DANORM) ? "" : NULL,
		},
		.ofile = {
			.name = ffsz_dup(oname),
		},
	};

	switch (rp.format) {
	case AF_AAC_HE:
		c.aac.profile = 'h';
		c.aac.quality = (uint)rp.quality;
		break;
	case AF_AAC_HE2:
		c.aac.profile = 'H';
		// fallthrough
	case AF_AAC_LC:
		c.aac.quality = (uint)rp.quality;
		break;

	case AF_MP3:
		c.iaudio.format.format = PHI_PCM_FLOAT32;
		c.mp3.quality = (uint)rp.quality + 1;
		break;

	case AF_OPUS:
	case AF_OPUS_VOICE:
		c.iaudio.format.format = PHI_PCM_FLOAT32;
		c.opus.bitrate = rp.quality;
		c.opus.mode = !!(rp.format == AF_OPUS_VOICE);
		break;
	}

	const phi_track_if *track = x->core->track;
	phi_track *t = track->create(&c);

	if (!track->filter(t, &phi_android_guard, 0)
		|| !track->filter(t, x->core->mod("core.auto-rec"), 0)
		|| !track->filter(t, x->core->mod("afilter.until"), 0)
		|| !track->filter(t, &phi_android_rec_ctl, 0)
		|| ((rp.flags & RECF_DANORM)
			&& !track->filter(t, x->core->mod("af-danorm.f"), 0))
		|| !track->filter(t, x->core->mod("afilter.gain"), 0)
		|| !track->filter(t, x->core->mod("afilter.auto-conv"), 0)
		|| !track->filter(t, x->core->mod("format.auto-write"), 0)
		|| !track->filter(t, x->core->mod("core.file-write"), 0))
		goto end;

	struct rec_ctx *rx = ffmem_new(struct rec_ctx);
	rx->RecordCallback.on_finish = jni_func(jni_class_obj(jcb), "on_finish", "(" JNI_TINT JNI_TSTR ")V");
	rx->RecordCallback.obj = jni_global_ref(jcb);
	t->udata = rx;

	t->output.allow_async = 1;
	track->start(t);
	e = 0;

end:
	jni_sz_free(dev_id, rp.src_preset);
	jni_sz_free(oname, joname);
	if (e != 0) {
		track->close(t);
		t = NULL;
	}
	dbglog("%s: exit", __func__);
	return (jlong)t;
}

enum {
	RECL_STOP = 1,
	RECL_PAUSE = 2,
	RECL_RESUME = 3,
};

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_Phiola_recCtrl(JNIEnv *env, jobject thiz, jlong trk, jint cmd)
{
	if (trk == 0) return NULL;

	jstring e = NULL;
	dbglog("%s: enter", __func__);

	phi_track *t = (void*)trk;
	struct rec_ctx *rx = t->udata;

	switch (cmd) {
	case RECL_STOP:
		if (t->error)
			e = jni_js_szf(env, "code %u", t->error);
		rx->state &= ~RS_PAUSED;
		x->core->track->stop(t);
		break;

	case RECL_PAUSE:
		rx->state |= RS_PAUSED; break;

	case RECL_RESUME:
		rx->state &= ~RS_PAUSED;
		x->core->track->wake(t);
		break;
	}

	dbglog("%s: exit", __func__);
	return e;
}
