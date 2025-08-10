/** phiola/Android: queue
2023, Simon Zolin */

static void qu_conv_update(phi_queue_id q);

static void list_redraw_delayed(void *param)
{
	JNIEnv *env;
	int r = jni_vm_attach(jvm, &env);
	if (r) {
		errlog("jni_vm_attach: %d", r);
		goto end;
	}

	jni_call_void(x->obj_QueueCallback, x->Phiola_QueueCallback_on_change, (jlong)x->q_adding, 'u', 0);

end:
	jni_vm_detach(jvm);
	x->q_adding = 0;
}

static void qu_on_change(phi_queue_id q, uint flags, uint pos)
{
	dbglog("%s: '%c' q:%p pos:%u", __func__, flags, (size_t)q, pos);

	switch (flags) {
	case 'a':
		if (q == x->q_adding)
			return; // redraw timer is already set
		if (!x->q_adding)  {
			x->q_adding = q;
			x->core->timer(0, &x->tmr_q_draw, -50, list_redraw_delayed, NULL);
			return; // delay redrawing this list
		}

		// redraw the previously modified list
		q = FF_SWAP(&x->q_adding, q);
		flags = 'u';
		pos = 0;
		break;

	case 'r':
	case 'c':
	case 'u':
	case 'm':
		break;

	case '.':
		if (q == x->convert.q && x->convert.interrupt)
			x->convert.interrupt = 2;
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
	x->queue.on_change(qu_on_change);
}

enum QUNF {
	QUNF_CONVERSION = 1,
};

JNIEXPORT jlong JNICALL
Java_com_github_stsaz_phiola_Phiola_quNew(JNIEnv *env, jobject thiz, jint flags)
{
	dbglog("%s: enter", __func__);
	struct phi_queue_conf c = {
		.first_filter = &phi_play_guard,
		.ui_module_if = &phi_play_ui,
		.ui_module_if_set = 1,
	};
	if (flags & QUNF_CONVERSION) {
		c.first_filter = &phi_mconvert_guard;
		c.conversion = 1;
		c.ui_module_if = &phi_convert_ui;
		c.ui_module_if_set = 1;
	}
	phi_queue_id q = x->queue.create(&c);
	dbglog("%s: exit", __func__);
	return (ffsize)q;
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_quDestroy(JNIEnv *env, jobject thiz, jlong q)
{
	dbglog("%s: enter", __func__);
	x->queue.destroy((phi_queue_id)q);
	dbglog("%s: exit", __func__);
}

#define QUADD_RECURSE  1

static void qu_cmd_add(struct core_data *d)
{
	struct phi_queue_entry qe = {
		.url = d->param_str,
	};
	x->queue.add(d->q, &qe);
	ffmem_free(d->param_str);
	ffmem_free(d);
}

static void qu_cmd_dup(struct core_data *d)
{
	phi_queue_id iq = (phi_queue_id)d->param_int;
	int pos = d->cmd;
	const struct phi_queue_entry *iqe;
	uint i = (pos >= 0) ? pos : 0;
	for (;  (iqe = x->queue.at(iq, i));  i++) {
		struct phi_queue_entry qe = {};
		qe_copy(&qe, iqe, &x->metaif);
		x->queue.add(d->q, &qe);
		if (pos >= 0)
			break;
	}
	ffmem_free(d);
}

static void qu_cmd_move(struct core_data *d)
{
	x->queue.move(d->cmd, d->param_int);
	ffmem_free(d);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_quMove(JNIEnv *env, jobject thiz, jint from, jint to)
{
	dbglog("%s: enter", __func__);
	struct core_data *d = ffmem_new(struct core_data);
	d->cmd = from;
	d->param_int = to;
	core_task(d, qu_cmd_move);
	dbglog("%s: exit", __func__);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_quDup(JNIEnv *env, jobject thiz, jlong jq, jlong q_src, jint pos)
{
	dbglog("%s: enter", __func__);
	struct core_data *d = ffmem_new(struct core_data);
	d->q = (phi_queue_id)jq;
	d->cmd = pos;
	d->param_int = q_src;
	core_task(d, qu_cmd_dup);
	dbglog("%s: exit", __func__);
}

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

		struct core_data *d = ffmem_new(struct core_data);
		d->q = (phi_queue_id)q;
		d->param_str = ffsz_dup(fn);
		core_task(d, qu_cmd_add);
	}

	jni_sz_free(fn, js);
	dbglog("%s: exit", __func__);
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_Phiola_quEntry(JNIEnv *env, jobject thiz, jlong q, jint i)
{
	struct phi_queue_entry *qe = x->queue.ref((phi_queue_id)q, i);
	const char *url = "";
	if (qe)
		url = qe->url;
	jstring s = jni_js_sz(url);
	if (qe)
		x->queue.unref(qe);
	return s;
}

JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_quMoveAll(JNIEnv *env, jobject thiz, jlong q, jstring jdst_dir)
{
	dbglog("%s: enter", __func__);
	uint n = 0, err = 0;
	const char *dst_dir = jni_sz_js(jdst_dir);
	char *newfn = NULL;

	for (uint i = 0;;  i++) {
		struct phi_queue_entry *qe = x->queue.ref((phi_queue_id)q, i);
		if (!qe)
			break;
		const char *url = qe->url;

		ffstr name;
		ffpath_splitpath_str(FFSTR_Z(url), NULL, &name);
		ffmem_free(newfn);
		newfn = ffsz_allocfmt("%s/%S", dst_dir, &name);

		if (fffile_exists(newfn)) {
			errlog("%s: '%s': target already exists", __func__, newfn);
			err++;
			goto next;
		}

		if (fffile_rename(url, newfn)) {
			syserrlog("fffile_rename: '%s' -> '%s'", url, newfn);
			err++;
			goto next;
		}
		n++;

next:
		x->queue.unref(qe);
	}

	ffmem_free(newfn);
	jni_sz_free(dst_dir, jdst_dir);
	dbglog("%s: exit", __func__);
	return (!err) ? n : -1;
}

JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_quRename(JNIEnv *env, jobject thiz, jlong q, jint pos, jstring jtarget, jint flags)
{
	int rc = 1;
	dbglog("%s: enter", __func__);
	const char *target = jni_sz_js(jtarget);

	char *fn = NULL;
	struct phi_queue_entry *qe;
	if (!(qe = x->queue.ref((phi_queue_id)q, pos)))
		goto end;

	ffstr name;
	ffpath_splitpath_str(FFSTR_Z(qe->url), NULL, &name);
	fn = ffsz_allocfmt("%s/%S", target, &name);

	rc = x->queue.rename(qe, fn, PHI_QRN_ACQUIRE);
	fn = NULL;
	x->queue.unref(qe);

end:
	ffmem_free(fn);
	jni_sz_free(target, jtarget);
	dbglog("%s: exit", __func__);
	return rc;
}

enum {
	QUCOM_CLEAR = 1,
	QUCOM_REMOVE_I = 2,
	QUCOM_COUNT = 3,
	QUCOM_INDEX = 4,
	QUCOM_SORT = 5,
	QUCOM_REMOVE_NON_EXISTING = 6,
	QUCOM_PLAY = 7,
	QUCOM_PLAY_NEXT = 8,
	QUCOM_PLAY_PREV = 9,
	QUCOM_META_READ = 10,
	QUCOM_CONV_CANCEL = 13,
	QUCOM_CONV_UPDATE = 14,
};

static void qc_apply(struct phi_queue_conf *qc)
{
	qc->repeat_all = x->play.repeat_all;
	qc->random = x->play.random;
	qc->tconf.afilter.rg_normalizer = (x->play.rg_normalizer && !x->play.auto_normalizer);
	qc->tconf.afilter.auto_normalizer = (x->play.auto_normalizer) ? "" : NULL;
}

static void qu_cmd(struct core_data *d)
{
	phi_queue_id q = d->q;
	switch (d->cmd) {
	case QUCOM_CLEAR:
		x->queue.clear(q);  break;

	case QUCOM_REMOVE_I:
		x->queue.remove_at(q, d->param_int, 1);  break;

	case QUCOM_REMOVE_NON_EXISTING:
		x->queue.remove_multi(q, PHI_Q_RM_NONEXIST);  break;

	case QUCOM_SORT:
		x->queue.sort(q, d->param_int);  break;

	case QUCOM_PLAY: {
		x->queue.qselect(q);
		qc_apply(x->queue.conf(NULL));
		x->queue.play(NULL, x->queue.at(q, d->param_int));
		break;
	}

	case QUCOM_PLAY_NEXT:
		x->queue.play(NULL, PHI_Q_PLAY_NEXT);  break;

	case QUCOM_PLAY_PREV:
		x->queue.play(NULL, PHI_Q_PLAY_PREVIOUS);  break;

	case QUCOM_META_READ:
		x->queue.read_meta(q);  break;

	case QUCOM_CONV_UPDATE:
		qu_conv_update(q);  break;
	}

	ffmem_free(d);
}

JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_quCmd(JNIEnv *env, jobject thiz, jlong jq, jint cmd, jint i)
{
	dbglog("%s: enter cmd:%u", __func__, cmd);
	int rc = 0;
	phi_queue_id q = (phi_queue_id)jq;

	switch (cmd) {
	case QUCOM_COUNT:
		rc = x->queue.count(q);  break;

	case QUCOM_INDEX: {
		struct phi_queue_entry *qe = x->queue.ref(q, i);
		rc = x->queue.index(qe);
		x->queue.unref(qe);
		break;
	}

	case QUCOM_CONV_CANCEL:
		FFINT_WRITEONCE(x->convert.interrupt, 1);  break;

	case QUCOM_CONV_UPDATE:
		rc = FFINT_READONCE(x->convert.n_tracks_updated);
		// fallthrough

	default: {
		struct core_data *d = ffmem_new(struct core_data);
		d->cmd = cmd;
		d->q = q;
		d->param_int = i;
		core_task(d, qu_cmd);
	}
	}

	dbglog("%s: exit", __func__);
	return rc;
}

enum {
	QC_REPEAT = 1,
	QC_RANDOM = 2,
	QC_REMOVE_ON_ERROR = 4,
	QC_AUTO_NORM = 0x10,
	QC_RG_NORM = 0x20,
};

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_quConf(JNIEnv *env, jobject thiz, jint mask, jint val)
{
	dbglog("%s: enter  mask:%u  val:%u", __func__, mask, val);

	if (mask & QC_REMOVE_ON_ERROR)
		x->play.remove_on_error = !!(val & QC_REMOVE_ON_ERROR);

	if (mask & QC_REPEAT)
		x->play.repeat_all = !!(val & QC_REPEAT);

	if (mask & QC_RANDOM)
		x->play.random = !!(val & QC_RANDOM);

	if (mask & QC_RG_NORM)
		x->play.rg_normalizer = !!(val & QC_RG_NORM);

	if (mask & QC_AUTO_NORM)
		x->play.auto_normalizer = !!(val & QC_AUTO_NORM);

	if (mask & (QC_REPEAT | QC_RANDOM | QC_RG_NORM | QC_AUTO_NORM)) {
		// Apply settings for the active playlist
		qc_apply(x->queue.conf(NULL));
	}

	dbglog("%s: exit", __func__);
}

static ffvec info_prepare(const struct phi_queue_entry *qe)
{
	const phi_meta *meta = &qe->meta;
	ffvec info = {};
	ffvec_allocT(&info, 5*2, char*);
	char **p = info.ptr;

	*p++ = ffsz_dup("url");
	*p++ = ffsz_dup(qe->url);

	*p++ = ffsz_dup("size");
	*p++ = NULL;

	*p++ = ffsz_dup("file time");
	*p++ = NULL;

	*p++ = ffsz_dup("length");
	*p++ = ffsz_allocfmt("%u:%02u", qe->length_sec / 60, qe->length_sec % 60);

	*p++ = ffsz_dup("format");
	ffstr val;
	if (!x->metaif.find(meta, FFSTR_Z("_phi_info"), &val, PHI_META_PRIVATE))
		*p++ = ffsz_dup(val.ptr);
	else
		*p++ = ffsz_dup("");

	info.len = p - (char**)info.ptr;

	uint i = 0;
	ffstr k, v;
	while (x->metaif.list(meta, &i, &k, &v, 0)) {
		ffvec_growT(&info, 2, char*);
		p = (char**)info.ptr + info.len;
		info.len += 2;
		*p++ = ffsz_dupstr(&k);
		if (ffstr_eqz(&k, "picture")) {
			*p = ffsz_dup("");
			continue;
		}
		*p = ffsz_dupstr(&v);
	}

	return info;
}

JNIEXPORT jobject JNICALL
Java_com_github_stsaz_phiola_Phiola_quMeta(JNIEnv *env, jobject thiz, jlong jq, jint i)
{
	phi_queue_id q = (phi_queue_id)jq;
	struct phi_queue_entry *qe = x->queue.ref(q, i);
	if (!qe)
		return NULL;
	fflock_lock((fflock*)&qe->lock); // core thread may read or write `conf.meta` at this moment
	ffvec info = info_prepare(qe);
	fflock_unlock((fflock*)&qe->lock);
	x->queue.unref(qe);

	char **p = (char**)info.ptr;
	enum {
		I_URL,
		I_SIZE,
		I_MTIME,
	};
	const char *fn = p[I_URL*2+1];
	fffileinfo fi;
	if (!fffile_info_path(fn, &fi)) {
		p[I_SIZE*2+1] = ffsz_allocfmt("%U KB", fffileinfo_size(&fi) / 1024);

		char mtime[100];
		ffdatetime dt = {};
		fftime t = fffileinfo_mtime(&fi);
		t.sec += FFTIME_1970_SECONDS; // UTC
		fftime_split1(&dt, &t);
		uint r = fftime_tostr1(&dt, mtime, sizeof(mtime), FFTIME_YMD);
		p[I_MTIME*2+1] = ffsz_dupn(mtime, r);
	} else {
		p[I_SIZE*2+1] = ffsz_dup("");
		p[I_MTIME*2+1] = ffsz_dup("");
	}

	jobject jmeta = jni_obj_new(x->Phiola_Meta, x->Phiola_Meta_init);
	jobjectArray jsa = jni_jsa_sza(env, info.ptr, info.len);
	jni_obj_jo_set(jmeta, jni_field(x->Phiola_Meta, "meta", JNI_TARR JNI_TSTR), jsa);

	FFSLICE_FOREACH_PTR_T(&info, ffmem_free, char*);
	ffvec_free(&info);

	return jmeta;
}

static void display_name_prepare(ffstr *val, ffsize cap, struct phi_queue_entry *qe, uint index, uint flags)
{
	ffstr artist = {}, title = {}, name;
	x->metaif.find(&qe->meta, FFSTR_Z("title"), &title, 0);
	if (title.len) {
		x->metaif.find(&qe->meta, FFSTR_Z("artist"), &artist, 0);
		ffstr_addfmt(val, cap, "%u. %S - %S"
			, index + 1, &artist, &title);
	} else {
		ffstr_setz(&name, qe->url);
		if (!url_checkz(qe->url))
			ffpath_splitpath_str(name, NULL, &name);
		ffstr_addfmt(val, cap, "%u. %S"
			, index + 1, &name);
	}

	if (!(flags & 1) // not a conversion track
		&& qe->length_sec) {
		uint sec = qe->length_sec;
		uint min = sec / 60;
		sec -= min * 60;
		ffstr_addfmt(val, cap, " [%u:%02u]"
			, min, sec);
	}
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_Phiola_quDisplayLine(JNIEnv *env, jobject thiz, jlong jq, jint i)
{
	dbglog("%s: enter %p %d", __func__, jq, i);
	phi_queue_id q = (phi_queue_id)jq;
	char buf[256];
	ffstr val = {};
	struct phi_queue_entry *qe = x->queue.ref(q, i);
	fflock_lock((fflock*)&qe->lock); // core thread may read or write `conf.meta` at this moment
	if (x->metaif.find(&qe->meta, FFSTR_Z("_phi_display"), &val, PHI_META_PRIVATE)) {
		val.ptr = buf;
		uint flags = x->queue.conf(q)->conversion;
		display_name_prepare(&val, sizeof(buf) - 1, qe, i, flags);
		x->metaif.set(&qe->meta, FFSTR_Z("_phi_display"), val, 0);
		val.ptr[val.len] = '\0';
	}
	jstring js = jni_js_sz(val.ptr);
	fflock_unlock((fflock*)&qe->lock);
	x->queue.unref(qe);
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
	phi_queue_id qf = x->queue.filter((phi_queue_id)q, FFSTR_Z(filter), flags);
	jni_sz_free(filter, jfilter);
	dbglog("%s: exit", __func__);
	return (jlong)qf;
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_Phiola_quConvertBegin(JNIEnv *env, jobject thiz, jlong jq, jobject jconf)
{
	dbglog("%s: enter", __func__);
	const char *error = "";
	jclass jc_conf = jni_class_obj(jconf);
	jstring jout_name = jni_obj_jo(jconf, jni_field_str(jc_conf, "out_name"));
	jstring jfrom = jni_obj_jo(jconf, jni_field_str(jc_conf, "from_msec"));
	jstring jto = jni_obj_jo(jconf, jni_field_str(jc_conf, "to_msec"));
	jstring jtags = jni_obj_jo(jconf, jni_field_str(jc_conf, "tags"));
	jstring jtrash_dir_rel = jni_obj_jo(jconf, jni_field_str(jc_conf, "trash_dir_rel"));
	uint flags = jni_obj_int(jconf, jni_field_int(jc_conf, "flags"));
	const char *ofn = jni_sz_js(jout_name)
		, *from = jni_sz_js(jfrom)
		, *to = jni_sz_js(jto)
		, *tags = jni_sz_js(jtags)
		, *trash_dir_rel = jni_sz_js(jtrash_dir_rel);

	struct phi_track_conf conf = {
		.ifile.preserve_date = !!(flags & COF_DATE_PRESERVE),
		.stream_copy = !!(flags & COF_COPY),
		.oaudio.format.format = jni_obj_int(jconf, jni_field_int(jc_conf, "sample_format")),
		.oaudio.format.rate = jni_obj_int(jconf, jni_field_int(jc_conf, "sample_rate")),

		.ofile.name = ffsz_dup(ofn),
		.ofile.name_tmp = 1,
		.ofile.overwrite = !!(flags & COF_OVERWRITE),

		.cross_worker_assign = 1,
	};

	int fmt = jni_obj_int(jconf, jni_field_int(jc_conf, "format"));
	switch (fmt) {
	case AF_AAC_LC:
		conf.aac.quality = jni_obj_int(jconf, jni_field_int(jc_conf, "aac_quality"));  break;

	case AF_OPUS:
		conf.opus.bitrate = jni_obj_int(jconf, jni_field_int(jc_conf, "opus_quality"));  break;
	}

	if (msec_apos(from, (int64*)&conf.seek_msec)) {
		error = "Incorrect 'from' value";
		goto end;
	}
	if (msec_apos(to, (int64*)&conf.until_msec)) {
		error = "Incorrect 'until' value";
		goto end;
	}

	ffstr s = FFSTR_INITZ(tags), name, val;
	while (s.len) {
		ffstr_splitby(&s, ';', &name, &s);
		ffstr_splitby(&name, '=', &name, &val);
		if (name.len)
			x->metaif.set(&conf.meta, name, val, 0);
	}

	ffmem_free(x->convert.trash_dir_rel);
	x->convert.trash_dir_rel = (trash_dir_rel[0]) ? ffsz_dup(trash_dir_rel) : NULL;

	x->convert.q_add_remove = (phi_queue_id)jni_obj_long(jconf, jni_field_long(jc_conf, "q_add_remove"));
	x->convert.q_add = !!(flags & COF_ADD);
	x->convert.q_pos = jni_obj_int(jconf, jni_field_int(jc_conf, "q_pos"));

	phi_queue_id q = (phi_queue_id)jq;
	struct phi_queue_entry *qe;
	if ((qe = x->queue.at(q, 0))) {
		struct phi_queue_conf *qc = x->queue.conf(q);
		ffmem_free(qc->tconf.ofile.name);
		x->metaif.destroy(&qc->tconf.meta);
		qc->tconf = conf;
		qc->tconf.meta = conf.meta;
		conf.meta = NULL;
		conf.ofile.name = NULL;
	}

	ffvec_free_align(&x->convert.tracks);
	uint n = x->queue.count(q);
	ffvec_alloc_alignT(&x->convert.tracks, n, 64, struct conv_track_info);
	ffmem_zero(x->convert.tracks.ptr, n * sizeof(struct conv_track_info));

	x->convert.q = q;
	x->convert.interrupt = 0;
	x->convert.n_tracks_updated = ~0U;
	if (qe)
		x->queue.play(NULL, x->queue.at(q, 0));

end:
	jni_sz_free(trash_dir_rel, jtrash_dir_rel);
	jni_sz_free(ofn, jout_name);
	jni_sz_free(from, jfrom);
	jni_sz_free(to, jto);
	jni_sz_free(tags, jtags);
	jstring js = jni_js_sz(error);
	dbglog("%s: exit", __func__);
	return js;
}

static void qu_conv_update(phi_queue_id q)
{
	struct phi_queue_entry *qe;
	uint n = 0;
	for (uint i = 0;  !!(qe = x->queue.at(q, i));  i++) {
		if (i >= x->convert.tracks.len) {
			if (x->convert.interrupt != 2) // the queue was not stopped by interrupt signal
				n++;
			break;
		}

		struct conv_track_info *cti = (struct conv_track_info*)x->convert.tracks.ptr + i;
		if (cti->final)
			continue;

		char buf[256];
		uint cap = sizeof(buf);
		ffstr val = {};
		val.ptr = buf;
		display_name_prepare(&val, cap, qe, i, 1);

		if (cti->error) {
			ffstr_addfmt(&val, cap, " [ERROR: %s]", cti->error);
			ffmem_free(cti->error);
			cti->error = NULL;
			cti->final = 1;

		} else if (cti->pos_sec == ~0U) {
			ffstr_addfmt(&val, cap, " [DONE]");
			cti->final = 1;

		} else {
			fflock_lock(&cti->lock); // if `cti->ct` is set, it is valid
			if (cti->ct)
				cti->pos_sec = cti->ct->pos_sec; // read current position from an active track
			fflock_unlock(&cti->lock);

			ffstr_addfmt(&val, cap, " [%u:%02u / %u:%02u]"
				, cti->pos_sec / 60, cti->pos_sec % 60
				, cti->duration_sec / 60, cti->duration_sec % 60);
			n++;
		}

		fflock_lock((fflock*)&qe->lock); // UI thread may read or write `conf.meta` at this moment
		x->metaif.set(&qe->meta, FFSTR_Z("_phi_display"), val, PHI_META_REPLACE);
		fflock_unlock((fflock*)&qe->lock);
	}
	FFINT_WRITEONCE(x->convert.n_tracks_updated, n);
}

JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_quLoad(JNIEnv *env, jobject thiz, jlong q, jstring jfilepath)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);

	struct core_data *d = ffmem_new(struct core_data);
	d->q = (phi_queue_id)q;
	d->param_str = ffsz_dup(fn);
	core_task(d, qu_cmd_add);

	jni_sz_free(fn, jfilepath);
	dbglog("%s: exit", __func__);
	return 0;
}

JNIEXPORT jboolean JNICALL
Java_com_github_stsaz_phiola_Phiola_quSave(JNIEnv *env, jobject thiz, jlong q, jstring jfilepath)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);
	x->queue.save((phi_queue_id)q, fn, NULL, NULL);
	jni_sz_free(fn, jfilepath);
	dbglog("%s: exit", __func__);
	return 1;
}
