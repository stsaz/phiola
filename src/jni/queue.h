/** phiola/Android: queue
2023, Simon Zolin */

static void qu_on_change(phi_queue_id q, uint flags, uint pos)
{
	dbglog("%s: '%c' q:%p", __func__, flags, (size_t)q);

	switch (flags) {
	case 'r':
	case 'c':
	case 'u':
		break;
	default:
		return;
	}

	JNIEnv *env;
	int r = jni_vm_attach(jvm, &env);
	if (r) {
		errlog("jni_vm_attach: %d", r);
		goto end;
	}

	jni_call_void(x->obj_QueueCallback, x->Phiola_QueueCallback_on_change, (jlong)q, flags, pos);

end:
	jni_vm_detach(jvm);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_quSetCallback(JNIEnv *env, jobject thiz, jobject jcb)
{
	x->Phiola_QueueCallback_on_change = jni_func(jni_class_obj(jcb), "on_change", "(" JNI_TLONG JNI_TINT JNI_TINT ")" JNI_TVOID);
	x->obj_QueueCallback = jni_global_ref(jcb);
	x->queue->on_change(qu_on_change);
}

JNIEXPORT jlong JNICALL
Java_com_github_stsaz_phiola_Phiola_quNew(JNIEnv *env, jobject thiz)
{
	struct phi_queue_conf c = {};
	return (ffsize)x->queue->create(&c);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_quDestroy(JNIEnv *env, jobject thiz, jlong q)
{
	x->queue->destroy((phi_queue_id)q);
}

#define QUADD_RECURSE  1

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_quAdd(JNIEnv *env, jobject thiz, jlong q, jobjectArray jurls, jint flags)
{
	dbglog("%s: enter", __func__);
	jstring js = NULL;
	const char *fn = NULL;
	ffsize n = jni_arr_len(jurls);
	for (uint i = 0;  i != n;  i++) {
		jni_sz_free(fn, js);
		js = jni_joa_i(jurls, i);
		fn = jni_sz_js(js);

		struct phi_queue_entry qe = {
			.conf.ifile.name = ffsz_dup(fn),
		};
		x->queue->add((phi_queue_id)q, &qe);
	}

	jni_sz_free(fn, js);
	dbglog("%s: exit", __func__);
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_Phiola_quEntry(JNIEnv *env, jobject thiz, jlong q, jint i)
{
	struct phi_queue_entry *qe = x->queue->ref((phi_queue_id)q, i);
	const char *url = qe->conf.ifile.name;
	jstring s = jni_js_sz(url);
	x->queue->unref(qe);
	return s;
}

#define QUCOM_CLEAR  1
#define QUCOM_REMOVE_I  2
#define QUCOM_COUNT  3
#define QUCOM_INDEX  4
#define QUCOM_SORT  5
#define QUCOM_REMOVE_NON_EXISTING  6

static void qu_cmd(struct core_data *d)
{
	switch (d->cmd) {
	case QUCOM_CLEAR:
		x->queue->clear(d->q);  break;

	case QUCOM_REMOVE_I:
		x->queue->remove_at(d->q, d->param_int, 1);  break;

	case QUCOM_REMOVE_NON_EXISTING:
		x->queue->remove_multi(d->q, PHI_Q_RM_NONEXIST);  break;

	case QUCOM_SORT:
		x->queue->sort(d->q, d->param_int);  break;
	}

	ffmem_free(d);
}

JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_quCmd(JNIEnv *env, jobject thiz, jlong jq, jint cmd, jint i)
{
	dbglog("%s: enter", __func__);
	int rc = 0;
	phi_queue_id q = (phi_queue_id)jq;

	switch (cmd) {
	case QUCOM_CLEAR:
	case QUCOM_REMOVE_I:
	case QUCOM_REMOVE_NON_EXISTING:
	case QUCOM_SORT: {
		struct core_data *d = ffmem_new(struct core_data);
		d->cmd = cmd;
		d->q = q;
		d->param_int = i;
		core_task(d, qu_cmd);
		break;
	}

	case QUCOM_COUNT:
		rc = x->queue->count(q);  break;

	case QUCOM_INDEX: {
		struct phi_queue_entry *qe = x->queue->ref(q, i);
		rc = x->queue->index(qe);
		x->queue->unref(qe);
		break;
	}
	}

	dbglog("%s: exit", __func__);
	return rc;
}

JNIEXPORT jobject JNICALL
Java_com_github_stsaz_phiola_Phiola_quMeta(JNIEnv *env, jobject thiz, jlong jq, jint i)
{
	phi_queue_id q = (phi_queue_id)jq;
	struct phi_queue_entry *qe = x->queue->ref(q, i);
	jobject jmeta = meta_create(env, &qe->conf.meta, qe->conf.ifile.name, qe->length_msec);
	x->queue->unref(qe);
	return jmeta;
}

static void display_name_prepare(ffstr *val, ffsize cap, struct phi_queue_entry *qe, uint index, uint flags)
{
	ffstr artist = {}, title = {}, name;
	x->metaif->find(&qe->conf.meta, FFSTR_Z("title"), &title, 0);
	if (title.len) {
		x->metaif->find(&qe->conf.meta, FFSTR_Z("artist"), &artist, 0);
		if (flags & 1) { // conversion
			ffstr_addfmt(val, cap, "%u. %S - %S"
				, index + 1, &artist, &title);
		} else {
			uint sec = qe->length_msec / 1000;
			uint min = sec / 60;
			sec -= min * 60;
			ffstr_addfmt(val, cap, "%u. %S - %S [%u:%02u]"
				, index + 1, &artist, &title
				, min, sec);
		}
	} else {
		ffpath_splitpath_str(FFSTR_Z(qe->conf.ifile.name), NULL, &name);
		ffstr_addfmt(val, cap, "%u. %S"
			, index + 1, &name);
	}
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_Phiola_quDisplayLine(JNIEnv *env, jobject thiz, jlong jq, jint i)
{
	dbglog("%s: enter", __func__);
	phi_queue_id q = (phi_queue_id)jq;
	char buf[256];
	ffstr val = {};
	struct phi_queue_entry *qe = x->queue->ref(q, i);
	if (x->metaif->find(&qe->conf.meta, FFSTR_Z("_phi_display"), &val, PHI_META_PRIVATE)) {
		val.ptr = buf;
		uint flags = x->queue->conf(q)->conversion;
		display_name_prepare(&val, sizeof(buf) - 1, qe, i, flags);
		x->metaif->set(&qe->conf.meta, FFSTR_Z("_phi_display"), val, 0);
		val.ptr[val.len] = '\0';
	}
	jstring js = jni_js_sz(val.ptr);
	x->queue->unref(qe);
	dbglog("%s: exit", __func__);
	return js;
}

enum {
	QUFILTER_URL = 1,
	QUFILTER_META = 2,
};

JNIEXPORT jlong JNICALL
Java_com_github_stsaz_phiola_Phiola_quFilter(JNIEnv *env, jobject thiz, jlong q, jstring jfilter, jint flags)
{
	dbglog("%s: enter", __func__);
	const char *filter = jni_sz_js(jfilter);
	phi_queue_id qf = x->queue->filter((phi_queue_id)q, FFSTR_Z(filter), flags);
	jni_sz_free(filter, jfilter);
	dbglog("%s: exit", __func__);
	return (jlong)qf;
}

JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_quLoad(JNIEnv *env, jobject thiz, jlong q, jstring jfilepath)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);
	struct phi_queue_entry qe = {
		.conf.ifile.name = ffsz_dup(fn),
	};
	x->queue->add((phi_queue_id)q, &qe);
	jni_sz_free(fn, jfilepath);
	dbglog("%s: exit", __func__);
	return 0;
}

JNIEXPORT jboolean JNICALL
Java_com_github_stsaz_phiola_Phiola_quSave(JNIEnv *env, jobject thiz, jlong q, jstring jfilepath)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);
	x->queue->save((phi_queue_id)q, fn, NULL, NULL);
	jni_sz_free(fn, jfilepath);
	dbglog("%s: exit", __func__);
	return 1;
}
