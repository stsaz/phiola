/** phiola/Android: recording functionality
2023, Simon Zolin */

static void rectrk_close(void *ctx, phi_track *t)
{
	JNIEnv *env;
	int r = jni_attach(jvm, &env);
	if (r != 0) {
		errlog("jni_attach: %d", r);
		goto end;
	}
	if (t->chain_flags & PHI_FFINISHED) {
		jobject jo = t->udata;
		jni_call_void(jo, x->Phiola_RecordCallback_on_finish);
	}

end:
	jni_global_unref(t->udata);
	jni_detach(jvm);
}

static int rectrk_process(void *ctx, phi_track *t)
{
	return PHI_DONE;
}

static const phi_filter phi_android_guard = {
	NULL, rectrk_close, rectrk_process,
	"rec-guard"
};

enum {
	REC_AACLC = 0,
	REC_AACHE = 1,
	REC_AACHE2 = 2,
	REC_FLAC = 3,
};

#define RECF_EXCLUSIVE  1
#define RECF_POWER_SAVE  2

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
	jint q = jni_obj_int(jconf, jni_field(jc_conf, "quality", JNI_TINT));
	jint until_sec = jni_obj_int(jconf, jni_field(jc_conf, "until_sec", JNI_TINT));
	jint flags = jni_obj_int(jconf, jni_field(jc_conf, "flags", JNI_TINT));

	uint aac_profile = 0;
	switch (fmt) {
	case REC_AACHE:
		aac_profile = 'h'; break;
	case REC_AACHE2:
		aac_profile = 'H'; break;
	}

	struct phi_track_conf c = {
		.iaudio = {
			.buf_time = buf_len_msec,
			.exclusive = !!(flags & RECF_EXCLUSIVE),
			.power_save = !!(flags & RECF_POWER_SAVE),
		},
		.until_msec = (uint)until_sec*1000,
		.afilter = {
			.gain_db = (double)gain_db100 / 100,
		},
		.aac = {
			.profile = aac_profile,
			.quality = (uint)q,
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
		|| !track->filter(t, x->core->mod("afilter.gain"), 0)
		|| !track->filter(t, x->core->mod("afilter.auto-conv"), 0)
		|| !track->filter(t, x->core->mod("format.auto-write"), 0)
		|| !track->filter(t, x->core->mod("core.file-write"), 0))
		goto end;

	x->Phiola_RecordCallback_on_finish = jni_func(jni_class_obj(jcb), "on_finish", "()V");
	t->udata = jni_global_ref(jcb);

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

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_recStop(JNIEnv *env, jobject thiz, jlong trk)
{
	if (trk == 0) return;

	dbglog("%s: enter", __func__);
	phi_track *t = (void*)trk;
	x->core->track->stop(t);
	dbglog("%s: exit", __func__);
}
