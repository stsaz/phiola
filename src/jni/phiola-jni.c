/** phiola/Android
2023, Simon Zolin */

#include <phiola.h>
#include <track.h>
#include <util/jni-helper.h>
#include <FFOS/process.h>
#include <android/log.h>

struct phiola_jni {
	phi_core *core;
	const phi_queue_if *queue;
	const phi_meta_if *metaif;
	jobject thiz;

	ffbyte debug;

	jmethodID Phiola_Callback_on_finish;
	jmethodID Phiola_Callback_on_convert_finish;
	jmethodID Phiola_Callback_on_info_finish;
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
	if (x->debug) \
		exe_log(NULL, PHI_LOG_DEBUG, NULL, NULL, __VA_ARGS__); \
} while (0)

#include <jni/record.h>
#include <jni/convert.h>
#include <jni/info.h>
#include <jni/queue.h>
#include <jni/android-utils.h>

static int conf()
{
#ifdef FF_DEBUG
	x->debug = 1;
#endif
	return 0;
}

static int core()
{
	struct phi_core_conf conf = {
		.log_level = (x->debug) ? PHI_LOG_DEBUG : PHI_LOG_VERBOSE,
		.log = exe_log,
		.logv = exe_logv,

		.run_detach = 1,
	};
	if (NULL == (x->core = phi_core_create(&conf)))
		return -1;
	x->queue = x->core->mod("core.queue");
	x->metaif = x->core->mod("format.meta");
	return 0;
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_init(JNIEnv *env, jobject thiz)
{
	if (x != NULL) return;

	x = ffmem_new(struct phiola_jni);
	if (!!conf()) return;
	core();
	phi_core_run();
	dbglog("%s: exit", __func__);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_destroy(JNIEnv *env, jobject thiz)
{
	if (x == NULL) return;

	dbglog("%s: enter", __func__);
	phi_core_destroy();
	ffmem_free(x);  x = NULL;
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

JNIEXPORT jint JNI_OnLoad(JavaVM *_jvm, void *reserved)
{
	jvm = _jvm;
	return JNI_VERSION_1_6;
}
