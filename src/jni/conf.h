/** phiola/Android: conf
2023, Simon Zolin */

#include <ffsys/file.h>

static const char setting_names[][20] = {
	"codepage",
	"conv_aac_q",
	"conv_copy",
	"conv_file_date_pres",
	"conv_format",
	"conv_mp3_q",
	"conv_new_add_list",
	"conv_opus_q",
	"conv_out_dir",
	"conv_out_name",
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
	"op_mlib_dir",
	"op_plist_save_dir",
	"op_trash_dir_rel",
	"play_auto_norm",
	"play_auto_skip",
	"play_auto_skip_tail",
	"play_auto_stop",
	"play_eqlz_enabled",
	"play_equalizer",
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
	"rec_src_preset",
	"rec_until",
	"ui_color",
	"ui_curpath",
	"ui_explorer_scroll",
	"ui_filter_hide",
	"ui_info_in_title",
	"ui_list_names",
	"ui_list_scroll_pos",
	"ui_mlib_scroll_pos",
	"ui_play_marker",
	"ui_play_marker_pos",
	"ui_record_hide",
	"ui_svc_notfn_disable",
	"ui_theme",
	"ui_view",
};

JNIEXPORT jboolean JNICALL
Java_com_github_stsaz_phiola_Conf_confRead(JNIEnv *env, jobject thiz, jstring jfilepath)
{
	int rc = 0;
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);
	ffvec d = {};
	if (fffile_readwhole(fn, &d, 1*1024*1024))
		goto end;
	jintArray jia = conf_read(env, *(ffstr*)&d, setting_names, FF_COUNT(setting_names), 0);

	jclass jc = jni_class_obj(thiz);
	jni_obj_jba_set(env, thiz, jni_field_jba(jc, "data"), *(ffstr*)&d);
	jni_obj_jo_set(thiz, jni_field(jc, "fields", JNI_TARR JNI_TINT), jia);
	rc = 1;

end:
	jni_sz_free(fn, jfilepath);
	ffvec_free(&d);
	dbglog("%s: exit", __func__);
	return rc;
}

JNIEXPORT jboolean JNICALL
Java_com_github_stsaz_phiola_Conf_confWrite(JNIEnv *env, jobject thiz, jstring jfilepath, jbyteArray jdata)
{
	dbglog("%s: enter", __func__);
	const char *fn = jni_sz_js(jfilepath);
	char *fn_tmp = ffsz_allocfmt("%s.tmp", fn);
	ffstr data = jni_str_jba(env, jdata);
	int rc = 0;
	if (fffile_writewhole(fn_tmp, data.ptr, data.len, 0)) {
		syserrlog("fffile_writewhole: %s", fn_tmp);
		goto end;
	}

	if (fffile_rename(fn_tmp, fn)) {
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
