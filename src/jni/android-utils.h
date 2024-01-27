/** phiola/Android: utils
2023, Simon Zolin */

#include <ffsys/path.h>
#include <ffsys/dir.h>

#define PJC_CONF_ENTRY  "com/github/stsaz/phiola/Conf$Entry"

static inline ffsize ffstr_charcount(ffstr s, int ch)
{
	ffsize r = 0;
	for (;;) {
		ffssize i = ffstr_findchar(&s, ch);
		if (i < 0)
			break;
		r++;
		ffstr_shift(&s, i+1);
	}
	return r;
}

static const char setting_names[][20] = {
	"codepage",
	"conv_aac_q",
	"conv_copy",
	"conv_file_date_pres",
	"conv_new_add_list",
	"conv_opus_q",
	"conv_outext",
	"conv_vorbis_q",
	"list_active",
	"list_add_rm_on_next",
	"list_curpos",
	"list_pos",
	"list_random",
	"list_repeat",
	"list_rm_on_err",
	"list_rm_on_next",
	"op_data_dir",
	"op_file_delete",
	"op_plist_save_dir",
	"op_quick_move_dir",
	"op_trash_dir_rel",
	"play_auto_skip",
	"play_auto_skip_tail",
	"play_auto_stop",
	"play_no_tags",
	"rec_bitrate",
	"rec_buf_len",
	"rec_channels",
	"rec_danorm",
	"rec_enc",
	"rec_exclusive",
	"rec_gain",
	"rec_path",
	"rec_rate",
	"rec_until",
	"ui_curpath",
	"ui_filter_hide",
	"ui_info_in_title",
	"ui_record_hide",
	"ui_state_hide",
	"ui_svc_notfn_disable",
	"ui_theme",
};

JNIEXPORT jobjectArray JNICALL
Java_com_github_stsaz_phiola_Conf_confRead(JNIEnv *env, jobject thiz, jstring jfilepath)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);
	ffvec d = {};
	jobjectArray joa = NULL;
	if (0 != fffile_readwhole(fn, &d, 1*1024*1024))
		goto end;

	jclass jc = jni_class(PJC_CONF_ENTRY);
	jmethodID jinit = jni_func(jc, "<init>", "()V");
	jfieldID jf_id = jni_field(jc, "id", JNI_TINT);
	jfieldID jf_value = jni_field(jc, "value", JNI_TSTR);

	ffstr s = FFSTR_INITSTR(&d);
	uint n = ffstr_charcount(s, '\n');
	joa = jni_joa(n, jc);

	uint i = 0;
	ffstr_setstr(&s, &d);
	while (s.len) {
		ffstr ln, k, v;
		ffstr_splitby(&s, '\n', &ln, &s);
		ffstr_splitby(&ln, ' ', &k, &v);

		int r = ffcharr_findsorted(setting_names, FF_COUNT(setting_names), sizeof(setting_names[0]), k.ptr, k.len);
		if (r < 0)
			continue;

		jobject jo = jni_obj_new(jc, jinit);

		jni_obj_int_set(jo, jf_id, r + 1);

		v.ptr[v.len] = '\0';
		jni_obj_sz_set(env, jo, jf_value, v.ptr);

		jni_joa_i_set(joa, i, jo);
		jni_local_unref(jo);
		i++;
	}

end:
	jni_sz_free(fn, jfilepath);
	ffvec_free(&d);
	dbglog("%s: exit", __func__);
	return joa;
}

JNIEXPORT jboolean JNICALL
Java_com_github_stsaz_phiola_Conf_confWrite(JNIEnv *env, jobject thiz, jstring jfilepath, jbyteArray jdata)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);
	char *fn_tmp = ffsz_allocfmt("%s.tmp", fn);
	ffstr data = jni_str_jba(env, jdata);
	int rc = 0;
	if (0 != fffile_writewhole(fn_tmp, data.ptr, data.len, 0)) {
		syserrlog("fffile_writewhole: %s", fn_tmp);
		goto end;
	}

	if (0 != fffile_rename(fn_tmp, fn)) {
		syserrlog("fffile_rename: %s", fn);
		goto end;
	}
	rc = 1;

end:
	jni_bytes_free(data.ptr, jdata);
	jni_sz_free(fn, jfilepath);
	ffmem_free(fn_tmp);
	dbglog("%s: exit", __func__);
	return rc;
}

static int file_trash(const char *trash_dir, const char *fn)
{
	int rc = -1;
	ffstr name;
	ffpath_splitpath_str(FFSTR_Z(fn), NULL, &name);
	char *trash_fn = ffsz_allocfmt("%s/%S", trash_dir, &name);

	uint flags = 1|2;
	for (;;) {

		if (fffile_exists(trash_fn)) {
			if (!(flags & 2))
				goto end;
			fftime now;
			fftime_now(&now);
			ffmem_free(trash_fn);
			trash_fn = ffsz_allocfmt("%s/%S-%xu", trash_dir, &name, (uint)now.sec);
			flags &= ~2;
			continue;
		}

		if (0 != fffile_rename(fn, trash_fn)) {
			syswarnlog("move to trash: %s", trash_fn);
			int e = fferr_last();
			if (fferr_notexist(e) && (flags & 1)) {
				ffdir_make(trash_dir);
				flags &= ~1;
				continue;
			}
			goto end;
		}

		dbglog("moved to trash: %s", fn);
		rc = 0;
		break;
	}

end:
	ffmem_free(trash_fn);
	return rc;
}

static char* trash_dir_abs(JNIEnv *env, jobjectArray jsa, const char *trash_dir_rel, const char *fn)
{
	char *trash_dir = NULL;
	// Select the storage root of the file to be moved
	jstring jstg = NULL;
	const char *stg = NULL;
	uint n = jni_arr_len(jsa);
	for (uint i = 0;  i != n;  i++) {
		jni_sz_free(stg, jstg);
		jni_local_unref(jstg);
		jstg = jni_joa_i(jsa, i);
		stg = jni_sz_js(jstg);
		if (ffsz_matchz(fn, stg)
			&& stg[0] != '\0' && fn[ffsz_len(stg)] == '/') {
			// e.g. "/storage/emulated/0/Music/file.mp3" starts with "/storage/emulated/0"
			trash_dir = ffsz_allocfmt("%s/%s", stg, trash_dir_rel);
			break;
		}
	}
	jni_sz_free(stg, jstg);
	jni_local_unref(jstg);
	return trash_dir;
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_UtilNative_trash(JNIEnv *env, jobject thiz, jstring jtrash_dir, jstring jfilepath)
{
	dbglog("%s: enter", __func__);
	jclass jc = jni_class_obj(thiz);
	const char *error = "";
	const char *trash_dir_rel = jni_sz_js(jtrash_dir);
	const char *fn = jni_sz_js(jfilepath);

	// Select the storage root of the file to be moved
	jobjectArray jsa = jni_obj_jo(thiz, jni_field(jc, "storage_paths", JNI_TARR JNI_TSTR));
	char *trash_dir = trash_dir_abs(env, jsa, trash_dir_rel, fn);

	if (trash_dir != NULL
		&& 0 != file_trash(trash_dir, fn))
		error = fferr_strptr(fferr_last());

	jni_sz_free(fn, jfilepath);
	jni_sz_free(trash_dir_rel, jtrash_dir);
	ffmem_free(trash_dir);

	jstring js = jni_js_sz(error);
	dbglog("%s: exit", __func__);
	return js;
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_UtilNative_fileMove(JNIEnv *env, jobject thiz, jstring jfilepath, jstring jtarget_dir)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);
	const char *tgt_dir = jni_sz_js(jtarget_dir);
	const char *error = "";

	ffstr fns = FFSTR_INITZ(fn);
	ffstr name;
	ffpath_splitpath_str(fns, NULL, &name);
	char *newfn = ffsz_allocfmt("%s/%S", tgt_dir, &name);
	if (fffile_exists(newfn))
		error = "file already exists";
	else if (0 != fffile_rename(fn, newfn))
		error = fferr_strptr(fferr_last());

	ffmem_free(newfn);
	jni_sz_free(fn, jfilepath);
	jni_sz_free(tgt_dir, jtarget_dir);

	jstring js = jni_js_sz(error);
	dbglog("%s: exit", __func__);
	return js;
}
