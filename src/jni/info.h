/** phiola/Android: info/meta tracks
2023, Simon Zolin */

static const char* channel_str(uint channels)
{
	static const char _channel_str[][8] = {
		"mono", "stereo",
		"3.0", "4.0", "5.0",
		"5.1", "6.1", "7.1"
	};
	channels = ffmin(channels - 1, FF_COUNT(_channel_str) - 1);
	return _channel_str[channels];
}

#define pcm_time(samples, rate)   ((uint64)(samples) * 1000 / (rate))

static uint64 info_add(ffvec *info, const phi_track *t)
{
	*ffvec_pushT(info, char*) = ffsz_dup("url");
	*ffvec_pushT(info, char*) = ffsz_dup(t->conf.ifile.name);

	*ffvec_pushT(info, char*) = ffsz_dup("size");
	*ffvec_pushT(info, char*) = ffsz_allocfmt("%UKB", t->input.size / 1024);

	*ffvec_pushT(info, char*) = ffsz_dup("file time");
	char mtime[100];
	ffdatetime dt = {};
	fftime_split1(&dt, &t->input.mtime);
	int r = fftime_tostr1(&dt, mtime, sizeof(mtime)-1, FFTIME_YMD);
	mtime[r] = '\0';
	*ffvec_pushT(info, char*) = ffsz_dup(mtime);

	uint64 msec = (t->audio.format.rate) ? pcm_time(t->audio.total, t->audio.format.rate) : 0;
	uint sec = msec / 1000;
	*ffvec_pushT(info, char*) = ffsz_dup("length");
	*ffvec_pushT(info, char*) = ffsz_allocfmt("%u:%02u.%03u (%,U samples)"
		, sec / 60, sec % 60, (int)(msec % 1000)
		, (int64)t->audio.total);

	return msec;
}

static const char ffaudio_formats_str[][8] = {
	"int8", "int16", "int24", "int32", "int24_4",
	"float32", "float64",
};
static const ushort ffpcm_formats[] = {
	PHI_PCM_8, PHI_PCM_16, PHI_PCM_24, PHI_PCM_32, PHI_PCM_24_4,
	PHI_PCM_FLOAT32, PHI_PCM_FLOAT64,
};
static inline const char* pcm_format_str(uint f)
{
	int i = ffarrint16_find(ffpcm_formats, FF_COUNT(ffpcm_formats), f);
	return ffaudio_formats_str[i];
}

void this_meta_fill(JNIEnv *env, jobject thiz, ffvec *meta, const char *filename, uint64 msec)
{
	char *artist, *title;
	ffstr val;
	if (!x->metaif->find(meta, FFSTR_Z("artist"), &val, 0))
		artist = ffsz_dupstr(&val);
	else
		artist = ffsz_dup("");

	if (!x->metaif->find(meta, FFSTR_Z("title"), &val, 0))
		title = ffsz_dupstr(&val);
	else
		title = ffsz_dup("");

	jclass jc = jni_class_obj(thiz);
	jfieldID jf;

	// this.url = ...
	jf = jni_field(jc, "url", JNI_TSTR);
	jni_obj_sz_set(env, thiz, jf, filename);

	// this.artist = ...
	jf = jni_field(jc, "artist", JNI_TSTR);
	jni_obj_sz_set(env, thiz, jf, artist);

	// this.title = ...
	jf = jni_field(jc, "title", JNI_TSTR);
	jni_obj_sz_set(env, thiz, jf, title);

	// this.length_msec = ...
	jf = jni_field(jc, "length_msec", JNI_TLONG);
	jni_obj_long_set(thiz, jf, msec);

	ffmem_free(artist);
	ffmem_free(title);
}

static ffvec info_prepare(JNIEnv *env, jobject thiz, jclass jc, phi_track *t)
{
	ffvec info = {};
	uint64 msec = info_add(&info, t);

	this_meta_fill(env, thiz, &t->meta, t->conf.ifile.name, msec);

	char *format = ffsz_allocfmt("%ukbps %s %uHz %s %s"
		, (t->audio.bitrate + 500) / 1000
		, t->audio.decoder
		, t->audio.format.rate
		, channel_str(t->audio.format.channels)
		, pcm_format_str(t->audio.format.format));
	jni_obj_sz_set(env, thiz, jni_field(jc, "info", JNI_TSTR), format);
	*ffvec_pushT(&info, char*) = ffsz_dup("format");
	*ffvec_pushT(&info, char*) = format;

	// info += t->meta
	char **it;
	FFSLICE_WALK(&t->meta, it) {
		*ffvec_pushT(&info, char*) = ffsz_dup(*it);
	}

	// * disable picture tag conversion attempt to UTF-8
	it = info.ptr;
	for (uint i = 0;  i != info.len;  i += 2) {
		if (ffsz_eq(it[i], "picture")) {
			char *val = it[i+1];
			val[0] = '\0';
		}
	}

	return info;
}

static void infotrk_close(void *ctx, phi_track *t)
{
	JNIEnv *env;
	int r = jni_attach(jvm, &env);
	if (r != 0) {
		errlog("jni_attach: %d", r);
		goto end;
	}

	if (t->chain_flags & PHI_FFINISHED) {
		jclass jc = jni_class_obj(x->thiz);
		ffvec info = info_prepare(env, x->thiz, jc, t);

		jobjectArray jsa = jni_jsa_sza(env, info.ptr, info.len);
		jni_obj_jo_set(x->thiz, jni_field(jc, "meta_data", JNI_TARR JNI_TSTR), jsa);

		char **it;
		FFSLICE_WALK(&info, it) {
			ffmem_free(*it);
		}
		ffvec_free(&info);

		jobject jo = t->udata;
		jni_call_void(jo, x->Phiola_Callback_on_info_finish);
	}

end:
	jni_global_unref(t->udata);
	jni_global_unref(x->thiz);
	jni_detach(jvm);
}

static int infotrk_process(void *ctx, phi_track *t)
{
	return PHI_DONE;
}

static const phi_filter phi_android_info_guard = {
	NULL, infotrk_close, infotrk_process,
	"info-guard"
};


JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_meta(JNIEnv *env, jobject thiz, jint list_item, jstring jfilepath, jobject jcb)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);

	struct phi_track_conf c = {
		.ifile.name = ffsz_dup(fn),
		.info_only = 1,
	};

	const phi_track_if *track = x->core->track;
	phi_track *t = track->create(&c);

	if (!track->filter(t, &phi_android_info_guard, 0)
		|| !track->filter(t, x->core->mod("core.file-read"), 0)
		|| !track->filter(t, x->core->mod("format.detect"), 0))
		goto end;

	x->Phiola_Callback_on_info_finish = jni_func(jni_class_obj(jcb), "on_finish", "()V");
	t->udata = jni_global_ref(jcb);
	x->thiz = jni_global_ref(thiz);

	track->start(t);
	t = NULL;

end:
	if (t != NULL)
		track->close(t);

	jni_sz_free(fn, jfilepath);
	dbglog("%s: exit", __func__);
	return 0;
}
