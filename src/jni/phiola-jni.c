/** phiola/Android
2023, Simon Zolin */

#include <phiola.h>
#include <track.h>
#include <util/jni-helper.h>
#include <util/util.h>
#include <ffsys/process.h>
#include <android/log.h>
#include <android/asset_manager_jni.h>

#define PJC_PHIOLA  "com/github/stsaz/phiola/Phiola"
#define PJC_META  "com/github/stsaz/phiola/Phiola$Meta"
#define PJT_META  "Lcom/github/stsaz/phiola/Phiola$Meta;"
#define PJC_UN_FILES  "com/github/stsaz/phiola/UtilNative$Files"

struct phiola_jni {
	phi_core *core;
	phi_queue_if queue;
	phi_meta_if metaif;

	ffstr dir_libs;
	ffbyte debug;
	ffvec storage_paths; // char*[]
	AAssetManager *am;

	struct {
		jmethodID PlayObserver_on_create;
		jmethodID PlayObserver_on_close;
		jmethodID PlayObserver_on_update;
		jobject obj_PlayObserver;

		phi_timer auto_stop_timer;
		phi_track *trk;
		int64 seek_msec;
		int auto_seek_sec_percent, auto_until_sec_percent;
		uint pos_prev_sec;
		uint opened :1;
		uint paused :1;
		uint remove_on_error :1;
		uint auto_stop_timer_expired :1;
		uint repeat_all :1;
		uint random :1;
	} play;

	struct {
		ffvec tracks; // struct conv_track_info[]
		phi_queue_id q, q_add_remove;
		char *trash_dir_rel;
		int q_pos;
		uint interrupt;
		uint n_tracks_updated;
	} convert;

	jclass Phiola_class;
	jmethodID Phiola_lib_load;

	jclass Phiola_Meta;
	jmethodID Phiola_Meta_init;

	jclass UtilNative_Files;
	jmethodID UtilNative_Files_init;

	jmethodID Phiola_QueueCallback_on_change;
	jobject obj_QueueCallback;
};
static struct phiola_jni *x;
static JavaVM *jvm;

static void exe_logv(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, va_list va)
{
	const char *id = (!!t) ? t->id : NULL;
	const char *ctx = (!!module || !t) ? module : (char*)x->core->track->cmd(t, PHI_TRACK_CUR_FILTER_NAME);

	ffuint level = flags & 0x0f;
	char buffer[4*1024];
	char *d = buffer;
	ffsize r = 0, cap = sizeof(buffer) - 3;

	if (ctx != NULL) {
		r += _ffs_copyz(&d[r], cap - r, ctx);
		d[r++] = ':';
		d[r++] = ' ';
	}

	if (id != NULL) {
		r += _ffs_copyz(&d[r], cap - r, id);
		d[r++] = ':';
		d[r++] = ' ';
	}

	ffssize r2 = ffs_formatv(&d[r], cap - r, fmt, va);
	if (r2 < 0)
		r2 = 0;
	r += r2;

	if (flags & PHI_LOG_SYS) {
		r += ffs_format_r0(&d[r], cap - r, ": (%u) %s"
			, fferr_last(), fferr_strptr(fferr_last()));
	}

#ifdef FF_WIN
	d[r++] = '\r';
#endif
	d[r++] = '\n';
	d[r++] = '\0';

	static const uint android_levels[] = {
		/*PHI_LOG_ERR*/		ANDROID_LOG_ERROR,
		/*PHI_LOG_WARN*/	ANDROID_LOG_WARN,
		/*PHI_LOG_USER*/	ANDROID_LOG_INFO,
		/*PHI_LOG_INFO*/	ANDROID_LOG_INFO,
		/*PHI_LOG_VERBOSE*/	ANDROID_LOG_INFO,
		/*PHI_LOG_DEBUG*/	ANDROID_LOG_DEBUG,
		/*PHI_LOG_EXTRA*/	ANDROID_LOG_DEBUG,
	};
	__android_log_print(android_levels[level], "phiola", "%s", d);
}

static void exe_log(void *log_obj, uint flags, const char *module, phi_track *t, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	exe_logv(log_obj, flags, module, t, fmt, va);
	va_end(va);
}

#define syserrlog(...) \
	exe_log(NULL, PHI_LOG_ERR | PHI_LOG_SYS, NULL, NULL, __VA_ARGS__)
#define errlog(...) \
	exe_log(NULL, PHI_LOG_ERR, NULL, NULL, __VA_ARGS__)
#define syswarnlog(...) \
	exe_log(NULL, PHI_LOG_WARN | PHI_LOG_SYS, NULL, NULL, __VA_ARGS__)

#define dbglog(...) \
do { \
	if (ff_unlikely(x->debug)) \
		exe_log(NULL, PHI_LOG_DEBUG, NULL, NULL, __VA_ARGS__); \
} while (0)

#define trk_dbglog(t, ...) \
do { \
	if (ff_unlikely(x->debug)) \
		exe_log(NULL, PHI_LOG_DEBUG, NULL, t, __VA_ARGS__); \
} while (0)


struct core_data {
	phi_task task;
	uint cmd;
	int64 param_int;
	phi_queue_id q;
};

static void core_task(struct core_data *d, void (*func)(struct core_data*))
{
	x->core->task(0, &d->task, (void(*)(void*))func, d);
}


enum {
	AF_AAC_LC = 0,
	AF_AAC_HE = 1,
	AF_AAC_HE2 = 2,
	AF_FLAC = 3,
	AF_OPUS = 4,
	AF_OPUS_VOICE = 5,
};

#include <jni/android-utils.h>
#include <jni/record.h>
#include <jni/convert.h>
#include <jni/playback.h>
#include <jni/queue.h>

static int conf()
{
#ifdef FF_DEBUG
	x->debug = 1;
#endif
	return 0;
}

/** Load modules via Java before dlopen().
Some modules also have the dependencies - load them too. */
static char* mod_loading(ffstr name)
{
	int e = -1;
	char* znames[3] = {};

	static const struct map_sz_vptr mod_deps[] = {
		{ "aac",	"libfdk-aac-phi" },
		{ "alac",	"libALAC-phi" },
		{ "danorm",	"libDynamicAudioNormalizer-phi" },
		{ "flac",	"libFLAC-phi" },
		{ "mpeg",	"libmpg123-phi" },
		{ "opus",	"libopus-phi" },
		{ "soxr",	"libsoxr-phi" },
		{ "vorbis",	"libvorbis-phi" },
		{ "zstd",	"libzstd-ffpack" },
		{}
	};
	const char *dep = map_sz_vptr_findstr(mod_deps, FF_COUNT(mod_deps), name);
	znames[0] = ffsz_allocfmt("%S/lib%S.so", &x->dir_libs, &name);
	if (dep) {
		znames[1] = ffsz_allocfmt("%S/%s.so", &x->dir_libs, dep);
		if (ffstr_eqz(&name, "vorbis"))
			znames[2] = ffsz_allocfmt("%S/libvorbisenc-phi.so", &x->dir_libs);
	}

	JNIEnv *env;
	int r, attached;
	if ((r = jni_vm_attach_once(jvm, &env, &attached, JNI_VERSION_1_6))) {
		errlog("jni_vm_attach: %d", r);
		goto end;
	}

	char **it;
	FF_FOREACH(znames, it) {
		if (!*it) break;

		if (!jni_scall_bool(x->Phiola_class, x->Phiola_lib_load, jni_js_sz(*it)))
			goto end;
		dbglog("loaded library %s", *it);
	}

	e = 0;

end:
	if (attached)
		jni_vm_detach(jvm);

	ffmem_free(znames[1]);
	ffmem_free(znames[2]);
	if (e) {
		ffmem_free(znames[0]);
		return NULL;
	}
	return znames[0];
}

static inline ffstr android_asset_read(AAssetManager *am, const char *path)
{
	ffstr s = {};

	AAsset *f = AAssetManager_open(am, path, AASSET_MODE_BUFFER);
	if (!f)
		goto end;

	uint64 n = AAsset_getLength64(f);
	ffstr_alloc(&s, n);
	s.len = AAsset_read(f, s.ptr, n);
	AAsset_close(f);

end:
	dbglog("%s: %s: %L", __func__, path, s.len);
	return s;
}

static ffstr resource_load(const char *name)
{
	return android_asset_read(x->am, name);
}

static int core()
{
	struct phi_core_conf conf = {
		.log_level = (x->debug) ? PHI_LOG_EXTRA : PHI_LOG_VERBOSE,
		.log = exe_log,
		.logv = exe_logv,

		.mod_loading = mod_loading,
		.resource_load = resource_load,

		.workers = ~0U,
		.io_workers = ~0U,
		.run_detach = 1,
	};
	if (NULL == (x->core = phi_core_create(&conf)))
		return -1;
	x->queue = *(phi_queue_if*)x->core->mod("core.queue");
	x->metaif = *(phi_meta_if*)x->core->mod("format.meta");
	return 0;
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_init(JNIEnv *env, jobject thiz, jstring jlibdir, jobject jasset_mgr)
{
	if (x != NULL) return;

	x = ffmem_new(struct phiola_jni);
	if (!!conf()) return;

	const char *libdir = jni_sz_js(jlibdir);
	x->dir_libs.ptr = ffsz_dup(libdir);
	jni_sz_free(libdir, jlibdir);
	x->dir_libs.len = ffsz_len(x->dir_libs.ptr);

	x->Phiola_class = jni_global_ref(jni_class(PJC_PHIOLA));
	x->Phiola_lib_load = jni_sfunc(x->Phiola_class, "lib_load", "(" JNI_TSTR ")" JNI_TBOOL);

	x->Phiola_Meta = jni_global_ref(jni_class(PJC_META));
	x->Phiola_Meta_init = jni_func(x->Phiola_Meta, "<init>", "()V");

	x->UtilNative_Files = jni_global_ref(jni_class(PJC_UN_FILES));
	x->UtilNative_Files_init = jni_func(x->UtilNative_Files, "<init>", "()V");

	FF_ASSERT(x->Phiola_class && x->Phiola_Meta && x->Phiola_lib_load);

	x->am = AAssetManager_fromJava(env, jasset_mgr);

	if (core()) return;
	phi_core_run();
	dbglog("%s: exit", __func__);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_destroy(JNIEnv *env, jobject thiz)
{
	if (x == NULL) return;

	dbglog("%s: enter", __func__);
	phi_core_destroy();
	jni_global_unref(x->play.obj_PlayObserver);
	jni_global_unref(x->obj_QueueCallback);
	jni_global_unref(x->Phiola_Meta);
	jni_global_unref(x->Phiola_class);

	char **it;
	FFSLICE_WALK(&x->storage_paths, it) {
		ffmem_free(*it);
	}
	ffvec_free(&x->storage_paths);

	// ffmem_free(conv_track_info.error);
	ffvec_free_align(&x->convert.tracks);
	ffmem_free(x->convert.trash_dir_rel);
	ffstr_free(&x->dir_libs);
	ffmem_free(x);  x = NULL;
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_Phiola_version(JNIEnv *env, jobject thiz)
{
	return jni_js_sz(x->core->version_str);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_setCodepage(JNIEnv *env, jobject thiz, jstring jcodepage)
{
	const char *sz = jni_sz_js(jcodepage);
	if (ffsz_eq(sz, "cp1251"))
		x->core->conf.code_page = FFUNICODE_WIN1251;
	else if (ffsz_eq(sz, "cp1252"))
		x->core->conf.code_page = FFUNICODE_WIN1252;
	jni_sz_free(sz, jcodepage);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_setDebug(JNIEnv *env, jobject thiz, jboolean enable)
{
	x->debug = !!enable;
	x->core->conf.log_level = (x->debug) ? PHI_LOG_EXTRA : PHI_LOG_VERBOSE;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *_jvm, void *reserved)
{
	jvm = _jvm;
	return JNI_VERSION_1_6;
}
