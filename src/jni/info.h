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

jobject meta_create(JNIEnv *env, ffvec *meta, const char *filename, uint64 msec)
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

	jclass jc = x->Phiola_Meta;
	jmethodID init = jni_func(jc, "<init>", "()V");
	jobject jmeta = jni_obj_new(jc, init);

	jni_obj_sz_set(env, jmeta, jni_field(jc, "url", JNI_TSTR), filename);
	jni_obj_sz_set(env, jmeta, jni_field(jc, "artist", JNI_TSTR), artist);
	jni_obj_sz_set(env, jmeta, jni_field(jc, "title", JNI_TSTR), title);
	jni_obj_int_set(jmeta, jni_field(jc, "length_msec", JNI_TINT), msec);

	ffmem_free(artist);
	ffmem_free(title);
	return jmeta;
}

static ffvec info_prepare(JNIEnv *env, jobject jmeta, jclass jc_meta, phi_track *t)
{
	ffvec info = {};
	info_add(&info, t);

	char *format = ffsz_allocfmt("%ukbps %s %uHz %s %s"
		, (t->audio.bitrate + 500) / 1000
		, t->audio.decoder
		, t->audio.format.rate
		, channel_str(t->audio.format.channels)
		, pcm_format_str(t->audio.format.format));
	jni_obj_sz_set(env, jmeta, jni_field(jc_meta, "info", JNI_TSTR), format);
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

static void meta_queue_store(phi_track *t)
{
	struct phi_queue_entry *qe = t->qent;
	x->metaif->destroy(&qe->conf.meta);
	qe->conf.meta = t->meta; // Remember the tags we read from file in this track
	qe->length_msec = (t->audio.format.rate) ? pcm_time(t->audio.total, t->audio.format.rate) : 0;
	ffvec_null(&t->meta);
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
		uint64 msec = (t->audio.format.rate) ? pcm_time(t->audio.total, t->audio.format.rate) : 0;
		jobject jmeta = meta_create(env, &t->meta, t->conf.ifile.name, msec);

		jclass jc = jni_class_obj(jmeta);
		ffvec info = info_prepare(env, jmeta, jc, t);
		meta_queue_store(t);

		jobjectArray jsa = jni_jsa_sza(env, info.ptr, info.len);
		jni_obj_jo_set(jmeta, jni_field(jc, "meta", JNI_TARR JNI_TSTR), jsa);

		char **it;
		FFSLICE_WALK(&info, it) {
			ffmem_free(*it);
		}
		ffvec_free(&info);

		jobject jo = t->udata;
		jni_call_void(jo, x->Phiola_MetaCallback_on_finish, jmeta);
	}

end:
	jni_global_unref(t->udata);
	jni_detach(jvm);
	x->queue->unref(t->qent);
	x->core->track->stop(t);
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
Java_com_github_stsaz_phiola_Phiola_meta(JNIEnv *env, jobject thiz, jlong q, jint list_item, jstring jfilepath, jobject jcb)
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

	t->qent = x->queue->ref((void*)q, list_item);

	x->Phiola_MetaCallback_on_finish = jni_func(jni_class_obj(jcb), "on_finish", "(" PJT_META ")V");
	t->udata = jni_global_ref(jcb);

	track->start(t);
	t = NULL;

end:
	if (t != NULL)
		track->close(t);

	jni_sz_free(fn, jfilepath);
	dbglog("%s: exit", __func__);
	return 0;
}
