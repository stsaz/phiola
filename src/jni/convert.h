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
	static const char errstr[][30] = {
		"Input file doesn't exist", // PHI_E_NOSRC
		"Output file already exists", // PHI_E_DSTEXIST
		"Unknown input file format", // PHI_E_UNKIFMT
		"Input audio device problem", // PHI_E_AUDIO_INPUT
	};
	const char *s = "Unknown";
	if (e < FF_COUNT(errstr))
		s = errstr[e];
	return s;
}


static void convtrk_close(void *ctx, phi_track *t)
{
	JNIEnv *env;
	int r = jni_attach(jvm, &env);
	if (r != 0) {
		errlog("jni_attach: %d", r);
		goto end;
	}

	if (t->chain_flags & PHI_FFINISHED) {
		jclass jc = jni_class_obj(x->thiz);
		if (t->error) {
			jni_obj_sz_setf(env, x->thiz, jni_field(jc, "result", JNI_TSTR), "ERROR: %s"
				, trk_errstr(t->error));
		} else {
			jni_obj_sz_set(env, x->thiz, jni_field(jc, "result", JNI_TSTR), "");
		}

		jobject jo = t->udata;
		jni_call_void(jo, x->Phiola_Callback_on_convert_finish);
	}

end:
	jni_global_unref(t->udata);
	jni_global_unref(x->thiz);
	jni_detach(jvm);
}

static int convtrk_process(void *ctx, phi_track *t)
{
	return PHI_DONE;
}

static const phi_filter phi_android_convert_guard = {
	NULL, convtrk_close, convtrk_process,
	"conv-guard"
};


#define F_DATE_PRESERVE  1
#define F_OVERWRITE  2

JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_convert(JNIEnv *env, jobject thiz, jstring jiname, jstring joname, jint flags, jobject jcb)
{
	dbglog("%s: enter", __func__);
	jclass jc = jni_class_obj(thiz);
	jstring jfrom = jni_obj_jo(thiz, jni_field(jc, "from_msec", JNI_TSTR));
	jstring jto = jni_obj_jo(thiz, jni_field(jc, "to_msec", JNI_TSTR));
	const char *ifn = jni_sz_js(jiname)
		, *ofn = jni_sz_js(joname)
		, *from = jni_sz_js(jfrom)
		, *to = jni_sz_js(jto);

	phi_track *t = NULL;
	const char *error = NULL;
	int64 seek = 0, until = 0;
	if (0 != msec_apos(from, &seek)) {
		error = "Please set correct 'from' value";
		goto end;
	}
	if (0 != msec_apos(to, &until)) {
		error = "Please set correct 'until' value";
		goto end;
	}

	struct phi_track_conf c = {
		.ifile = {
			.name = ffsz_dup(ifn),
			.preserve_date = !!(flags & F_DATE_PRESERVE),
		},
		.seek_msec = seek,
		.until_msec = until,
		.aac = {
			.quality = jni_obj_int(thiz, jni_field(jc, "aac_quality", JNI_TINT)),
		},
		.ofile = {
			.name = ffsz_dup(ofn),
			.overwrite = !!(flags & F_OVERWRITE),
		},
		.stream_copy = jni_obj_bool(thiz, jni_field(jc, "copy", JNI_TBOOL)),
	};

	const phi_track_if *track = x->core->track;
	t = track->create(&c);

	if (!track->filter(t, &phi_android_convert_guard, 0)
		|| !track->filter(t, x->core->mod("core.file-read"), 0)
		|| !track->filter(t, x->core->mod("format.detect"), 0)
		|| !track->filter(t, x->core->mod("afilter.until"), 0)
		|| !track->filter(t, x->core->mod("afilter.auto-conv"), 0)
		|| !track->filter(t, x->core->mod("format.auto-write"), 0)
		|| !track->filter(t, x->core->mod("core.file-write"), 0)) {
		error = "modules not available";
		goto end;
	}

	x->Phiola_Callback_on_convert_finish = jni_func(jni_class_obj(jcb), "on_finish", "()V");
	t->udata = jni_global_ref(jcb);
	x->thiz = jni_global_ref(thiz);

	track->start(t);
	t = NULL;

end:
	if (t != NULL)
		track->close(t);

	if (error != NULL)
		jni_obj_sz_setf(env, thiz, jni_field(jc, "result", JNI_TSTR), "ERROR: %s"
			, error);

	jni_sz_free(ifn, jiname);
	jni_sz_free(ofn, joname);
	jni_sz_free(from, jfrom);
	jni_sz_free(to, jto);
	dbglog("%s: exit", __func__);
	return !(error == NULL);
}
