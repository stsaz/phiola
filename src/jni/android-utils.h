/** phiola/Android: utils
2023, Simon Zolin */

#include <ffsys/path.h>
#include <ffsys/dir.h>
#include <ffsys/dirscan.h>

#define PJC_CONF_ENTRY  "com/github/stsaz/phiola/Conf$Entry"

static const char setting_names[][20] = {
	"codepage",
	"conv_aac_q",
	"conv_copy",
	"conv_file_date_pres",
	"conv_format",
	"conv_new_add_list",
	"conv_opus_q",
	"conv_out_dir",
	"conv_out_name",
	"conv_vorbis_q",
	"list_active",
	"list_add_rm_on_next",
	"list_curpos",
	"list_random",
	"list_repeat",
	"list_rm_on_err",
	"list_rm_on_next",
	"op_data_dir",
	"op_deprecated_mods",
	"op_file_delete",
	"op_plist_save_dir",
	"op_quick_move_dir",
	"op_trash_dir_rel",
	"play_auto_norm",
	"play_auto_skip",
	"play_auto_skip_tail",
	"play_auto_stop",
	"play_rg_norm",
	"rec_bitrate",
	"rec_buf_len",
	"rec_channels",
	"rec_danorm",
	"rec_enc",
	"rec_exclusive",
	"rec_gain",
	"rec_list_add",
	"rec_longclick",
	"rec_name",
	"rec_path",
	"rec_rate",
	"rec_src_unproc",
	"rec_until",
	"ui_curpath",
	"ui_filter_hide",
	"ui_info_in_title",
	"ui_list_names",
	"ui_list_scroll_pos",
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
	jfieldID jf_value = jni_field(jc, "value", JNI_TSTR);
	jfieldID jf_number = jni_field(jc, "number", JNI_TINT);
	jfieldID jf_enabled = jni_field(jc, "enabled", JNI_TBOOL);

	ffstr s = FFSTR_INITSTR(&d);
	joa = jni_joa(FF_COUNT(setting_names)+1, jc);

	ffstr_setstr(&s, &d);
	while (s.len) {
		ffstr ln, k, v;
		ffstr_splitby(&s, '\n', &ln, &s);
		ffstr_splitby(&ln, ' ', &k, &v);

		int r = ffcharr_findsorted(setting_names, FF_COUNT(setting_names), sizeof(setting_names[0]), k.ptr, k.len);
		if (r < 0)
			continue;

		jobject jo = jni_obj_new(jc, jinit);

		v.ptr[v.len] = '\0';
		jni_obj_sz_set(env, jo, jf_value, v.ptr);

		int n;
		if (ffstr_to_int32(&v, &n)) {
			jni_obj_int_set(jo, jf_number, n);
			jni_obj_bool_set(jo, jf_enabled, (n == 1));
		}

		jni_joa_i_set(joa, r + 1, jo);
		jni_local_unref(jo);
	}

	for (uint i = 1;  i <= FF_COUNT(setting_names);  i++) {
		jobject jo = jni_joa_i(joa, i);
		if (!jo) {
			jo = jni_obj_new(jc, jinit);
			jni_obj_sz_set(env, jo, jf_value, "");
			jni_joa_i_set(joa, i, jo);
		}
		jni_local_unref(jo);
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

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_UtilNative_storagePaths(JNIEnv *env, jobject thiz, jobjectArray jpaths)
{
	jstring jstg = NULL;
	const char *stg = NULL;
	uint n = jni_arr_len(jpaths);
	ffvec_allocT(&x->storage_paths, n, char*);
	for (uint i = 0;  i != n;  i++) {
		jstg = jni_joa_i(jpaths, i);
		stg = jni_sz_js(jstg);
		*ffvec_pushT(&x->storage_paths, char*) = ffsz_dup(stg);
		jni_sz_free(stg, jstg);
		jni_local_unref(jstg);
	}
}

static char* trash_dir_abs(const char *trash_dir_rel, const char *fn)
{
	const char **it;
	FFSLICE_WALK(&x->storage_paths, it) {
		const char *stg = *it;
		if (ffsz_matchz(fn, stg)
			&& stg[0] != '\0' && fn[ffsz_len(stg)] == '/') {
			// e.g. "/storage/emulated/0/Music/file.mp3" starts with "/storage/emulated/0"
			return ffsz_allocfmt("%s/%s", stg, trash_dir_rel);
		}
	}
	return NULL;
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_UtilNative_trash(JNIEnv *env, jobject thiz, jstring jtrash_dir, jstring jfilepath)
{
	dbglog("%s: enter", __func__);
	const char *error = "";
	const char *trash_dir_rel = jni_sz_js(jtrash_dir);
	const char *fn = jni_sz_js(jfilepath);

	// Select the storage root of the file to be moved
	char *trash_dir = trash_dir_abs(trash_dir_rel, fn);
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

JNIEXPORT jobject JNICALL
Java_com_github_stsaz_phiola_UtilNative_dirList(JNIEnv *env, jobject thiz, jstring jpath, jint flags)
{
	dbglog("%s: enter", __func__);
	const char *path = jni_sz_js(jpath);
	char *fullname = NULL, *fullname_name;
	size_t i = 0, n = 0, ndirs = 0;
	ffdirscanx dx = {};

	if (!ffdirscanx_open(&dx, path, FFDIRSCANX_SORT_DIRS))
		n = ffdirscan_count(&dx.ds);

	ffstr s_path = FFSTR_INITZ(path);
	fullname = ffmem_alloc(s_path.len + 1 + 255);
	ffmem_copy(fullname, s_path.ptr, s_path.len);
	fullname[s_path.len] = '/';
	fullname_name = fullname + s_path.len + 1;

	jclass jcs = jni_class(JNI_CSTR);
	jobjectArray jsa_names = jni_joa(n, jcs);
	jobjectArray jsa_rows = jni_joa(n, jcs);

	const char *fn;
	while ((fn = ffdirscanx_next(&dx))) {
		ffsz_copyz(fullname_name, 255, fn);
		jstring js = jni_js_sz(fullname);
		jni_joa_i_set(jsa_names, i, js);
		jni_local_unref(js);

		uint off = *(uint*)((char*)dx.ds.names + dx.ds.cur - sizeof(uint));
		if (off & 0x80000000) {
			js = jni_js_szf(env, "<DIR> %s", fn);
			ndirs++;
		} else {
			js = jni_js_sz(fn);
		}
		jni_joa_i_set(jsa_rows, i, js);
		jni_local_unref(js);

		i++;
	}

	jobject jo = jni_obj_new(x->UtilNative_Files, x->UtilNative_Files_init);
	jni_obj_jo_set(jo, jni_field(x->UtilNative_Files, "file_names", JNI_TARR JNI_TSTR), jsa_names);
	jni_obj_jo_set(jo, jni_field(x->UtilNative_Files, "display_rows", JNI_TARR JNI_TSTR), jsa_rows);
	jni_obj_int_set(jo, jni_field_int(x->UtilNative_Files, "n_directories"), ndirs);

	ffdirscanx_close(&dx);
	jni_sz_free(path, jpath);
	ffmem_free(fullname);
	dbglog("%s: exit", __func__);
	return jo;
}
