/** phiola/Android: recording functionality
2023, Simon Zolin */

struct rec_ctx {
	uint state;

	jmethodID Phiola_RecordCallback_on_finish;
	jobject on_finish_object;
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
	jni_call_void(rx->on_finish_object, rx->Phiola_RecordCallback_on_finish, t->error, joname);

end:
	jni_global_unref(rx->on_finish_object);
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
#define RECF_DANORM  4

JNIEXPORT jlong JNICALL
Java_com_github_stsaz_phiola_Phiola_recStart(JNIEnv *env, jobject thiz, jstring joname, jobject jconf, jobject jcb)
{
	dbglog("%s: enter", __func__);
	int e = -1;
	const char *oname = jni_sz_js(joname);

	jclass jc_conf = jni_class_obj(jconf);
	jint buf_len_msec = jni_obj_int(jconf, jni_field(jc_conf, "buf_len_msec", JNI_TINT));
	jint gain_db100 = jni_obj_int(jconf, jni_field(jc_conf, "gain_db100", JNI_TINT));
	jint fmt = jni_obj_int(jconf, jni_field(jc_conf, "format", JNI_TINT));
	jint chan = jni_obj_int(jconf, jni_field(jc_conf, "channels", JNI_TINT));
	jint rate = jni_obj_int(jconf, jni_field(jc_conf, "sample_rate", JNI_TINT));
	jint q = jni_obj_int(jconf, jni_field(jc_conf, "quality", JNI_TINT));
	jint until_sec = jni_obj_int(jconf, jni_field(jc_conf, "until_sec", JNI_TINT));
	jint flags = jni_obj_int(jconf, jni_field(jc_conf, "flags", JNI_TINT));

	struct phi_track_conf c = {
		.iaudio = {
			.format = {
				.channels = chan,
				.rate = rate,
			},
			.buf_time = buf_len_msec,
			.exclusive = !!(flags & RECF_EXCLUSIVE),
			.power_save = !!(flags & RECF_POWER_SAVE),
		},
		.until_msec = (uint)until_sec*1000,
		.afilter = {
			.gain_db = (double)gain_db100 / 100,
			.danorm = (flags & RECF_DANORM) ? "" : NULL,
		},
		.aac = {
			.profile = (fmt == AF_AAC_HE) ? 'h'
				: (fmt == AF_AAC_HE2) ? 'H'
				: 0,
			.quality = (uint)q,
		},
		.opus = {
			.bitrate = q,
			.mode = (fmt == AF_OPUS_VOICE) ? 1 : 0,
		},
		.ofile = {
			.name = ffsz_dup(oname),
		},
	};

	const phi_track_if *track = x->core->track;
	phi_track *t = track->create(&c);

	if (!track->filter(t, &phi_android_guard, 0)
		|| !track->filter(t, x->core->mod("core.auto-rec"), 0)
		|| !track->filter(t, x->core->mod("afilter.until"), 0)
		|| !track->filter(t, &phi_android_rec_ctl, 0)
		|| ((flags & RECF_DANORM)
			&& !track->filter(t, x->core->mod("danorm.f"), 0))
		|| !track->filter(t, x->core->mod("afilter.gain"), 0)
		|| !track->filter(t, x->core->mod("afilter.auto-conv"), 0)
		|| !track->filter(t, x->core->mod("format.auto-write"), 0)
		|| !track->filter(t, x->core->mod("core.file-write"), 0))
		goto end;

	struct rec_ctx *rx = ffmem_new(struct rec_ctx);
	rx->Phiola_RecordCallback_on_finish = jni_func(jni_class_obj(jcb), "on_finish", "(" JNI_TINT JNI_TSTR ")V");
	rx->on_finish_object = jni_global_ref(jcb);
	t->udata = rx;

	t->output.allow_async = 1;
	track->start(t);
	e = 0;

end:
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
