/** phiola/Android: utils
2023, Simon Zolin */

#include <ffsys/path.h>
#include <ffsys/dir.h>
#include <ffsys/dirscan.h>

/** Read config data into Java array.
(KEY VALUE LF)... -> {value-offset value-length value-as-number}...
*/
jintArray conf_read(JNIEnv *env, ffstr data, const char settings[][20], uint n_settings, int int_default)
{
	const char *data_start = data.ptr;
	ffvec fields = {};
	ffvec_zalloc(&fields, n_settings * 3, 4);
	fields.len = n_settings * 3;
	int *dst = fields.ptr;

	while (data.len) {
		ffstr ln, k, v;
		ffstr_splitby(&data, '\n', &ln, &data);
		ffstr_splitby(&ln, ' ', &k, &v);

		int r = ffcharr_findsorted(settings, n_settings, sizeof(settings[0]), k.ptr, k.len);
		if (r < 0)
			continue;

		dst[r*3 + 0] = v.ptr - data_start;
		dst[r*3 + 1] = v.len;

		int n = int_default;
		ffstr_to_int32(&v, &n);
		dst[r*3 + 2] = n;
	}

	jintArray jia = jni_jia_vec(env, *(ffslice*)&fields);
	ffvec_free(&fields);
	return jia;
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

JNIEXPORT jobject JNICALL
Java_com_github_stsaz_phiola_UtilNative_dirList(JNIEnv *env, jobject thiz, jstring jpath, jint flags)
{
	dbglog("%s: enter", __func__);
	const char *path = jni_sz_js(jpath);
	char *fullname = NULL, *fullname_name;
	size_t i = 0, n = 0, ndirs = 0;
	ffdirscanx dx = {};

	uint dsof = FFDIRSCANX_SORT_DIRS;
	if (flags & 1) {
		dx.ds.wildcard = "*.m3u*";
		dsof = FFDIRSCAN_USEWILDCARD;
	}

	if (!ffdirscanx_open(&dx, path, dsof))
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
		uint off = *(uint*)((char*)dx.ds.names + dx.ds.cur - sizeof(uint));

		if (flags & 1) {
			if (off & 0x80000000)
				continue; // skip dirs
			ffstr ext;
			ffpath_split3_str(FFSTR_Z(fn), NULL, NULL, &ext);
			if (!(ffstr_ieqz(&ext, "m3u")
				|| ffstr_ieqz(&ext, "m3u8")))
				continue;
		}

		ffsz_copyz(fullname_name, 255, fn);
		jstring js = jni_js_sz(fullname);
		jni_joa_i_set(jsa_names, i, js);
		jni_local_unref(js);

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
