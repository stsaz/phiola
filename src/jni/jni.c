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
#define PJC_RECINFO  "com/github/stsaz/phiola/Phiola$RecInfo"
#define PJC_UN_FILES  "com/github/stsaz/phiola/UtilNative$Files"

struct Callbacks {
	jobject obj;
	jmethodID play_new;
	jmethodID play_fin;
	jmethodID play_update;
	jmethodID recording;
};

static const struct jni_ifmap Callbacks_map[] = {
	{ "play_new", "(" PJT_META ")" JNI_TVOID },
	{ "play_fin", "(" JNI_TINT ")" JNI_TVOID },
	{ "play_update", "(" JNI_TLONG ")" JNI_TVOID },
	{ "recording", "(" JNI_TINT JNI_TINT JNI_TSTR ")" JNI_TVOID },
	{}
};

struct QueueCallback {
	jobject obj;
	jmethodID on_change;
	jmethodID on_complete;
};

static const struct jni_ifmap QueueCallback_map[] = {
	{ "on_change", "(" JNI_TLONG JNI_TINT JNI_TINT ")" JNI_TVOID },
	{ "on_complete", "(" JNI_TINT JNI_TINT ")" JNI_TVOID },
	{}
};

struct Config {
	jstring		codepage;
	jstring		equalizer;
	jint		queue_flags;
	jint		auto_seek;
	jint		auto_until;
	jboolean	deprecated_mods;
};

#define _S(name)  { #name, 's', FF_OFF(struct Config, name), 0 }
#define _I(name)  { #name, 'i', FF_OFF(struct Config, name), 0 }
#define _Z(name)  { #name, 'z', FF_OFF(struct Config, name), 0 }
static struct jni_cmap Config_map[] = {
	_S(codepage),
	_S(equalizer),
	_I(queue_flags),
	_I(auto_seek),
	_I(auto_until),
	_Z(deprecated_mods),
	{},
};
#undef _S
#undef _I
#undef _Z

struct phiola_jni {
	phi_core *core;
	phi_queue_if queue;
	phi_meta_if metaif;

	u_char debug;
	ffvec storage_paths; // char*[]
	AAssetManager *am;

	struct {
		phi_timer auto_stop_timer;
		phi_track *trk;
		char *equalizer;
		char *tee_filename;
		int64 seek_msec;
		int auto_seek_sec_percent, auto_until_sec_percent;
		uint pos_prev_sec;
		uint opened :1;
		uint paused :1;
		uint remove_on_error :1;
		uint auto_stop_timer_expired :1;
		uint repeat_all :1;
		uint random :1;
		uint rg_normalizer :1;
		uint auto_normalizer :1;
	} play;

	struct {
		struct jni_class_t RecInfo;
		char *device_id;
	} rec;

	struct {
		ffvec tracks; // struct conv_track_info[]
		phi_queue_id q, q_add_remove;
		char *trash_dir_rel;
		int q_pos;
		uint interrupt;
		uint n_tracks_updated;
		uint q_add :1;
	} convert;

	const phi_tag_if *tag_if;

	phi_timer tmr_q_draw;
	phi_queue_id q_adding;

	jclass Phiola_class;
	jmethodID Phiola_lib_load;

	struct jni_class_t Meta;
	struct jni_class_t UtilNative_Files;
	struct QueueCallback QueueCallback;
	struct Config Config;
	struct Callbacks Callbacks;
};
static struct phiola_jni *x;
static JavaVM *jvm;

#define jni_phi_track_allocT(t, T)  x->core->track->memalloc(t, sizeof(T))
#define jni_phi_track_free(t, ptr)  x->core->track->memfree(t, ptr)


struct core_data {
	phi_task task;
	uint cmd;
	union {
		int64 param_int;
		char *param_str;
	};
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
	AF_MP3 = 6,
	AF_WAV = 7,
	AF_FLAC24 = 8,
};

#include <jni/log.h>
#include <jni/android-utils.h>
#include <jni/conf.h>
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
	int e = -1, r, attached = 0;
	char* znames[2] = {};
	JNIEnv *env;

	static const struct {
		char module[16];
		char dependency[31];
		u_char deprecated;
	} mods[] = {
		{ "ac-aac",		"fdk-aac-phi",	0 },
		{ "ac-alac",	"ALAC-phi",		1 },
		{ "ac-flac",	"FLAC-phi",		0 },
		{ "ac-mp3lame",	"mp3lame-phi",	0 },
		{ "ac-mpeg",	"mpg123-phi",	0 },
		{ "ac-opus",	"opus-phi",		0 },
		{ "ac-vorbis",	"vorbis-phi",	0 },
		{ "af-danorm",	"DynamicAudioNormalizer-phi",	0 },
		{ "af-loudness","ebur128-phi",	0 },
		{ "af-sox",		"sox-phi",		0 },
		{ "af-soxr",	"soxr-phi",		0 },
		{ "zstd",		"zstd-ffpack",	0 },
	};
	int i = ffcharr_find_sorted_padding(mods, FF_COUNT(mods), sizeof(mods[0].module), sizeof(mods[0]) - sizeof(mods[0].module), name.ptr, name.len);
	znames[0] = ffsz_dupstr(&name);
	if (i >= 0) {

		if (mods[i].deprecated
			&& !x->Config.deprecated_mods) {
			errlog("Loading deprecated libraries is restricted: %S", &name);
			goto end;
		}

		znames[1] = ffsz_dup(mods[i].dependency);
	}

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

	ffmem_free(znames[0]);
	ffmem_free(znames[1]);
	if (e)
		return NULL;
	return ffsz_allocfmt("lib%S.so", &name);
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
		.tee_out_first = &tee_out_guard,

		.workers = ~0U,
		.io_workers = ~0U,
		.run_detach = 1,
	};
	if (NULL == (x->core = phi_core_create(&conf)))
		return -1;
	x->queue = *(phi_queue_if*)x->core->mod("core.queue");
	x->metaif = *(phi_meta_if*)x->core->metaif;
	return 0;
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_init(JNIEnv *env, jobject thiz, jobject jasset_mgr)
{
	if (x != NULL) return;

	x = ffmem_new(struct phiola_jni);
	if (!!conf()) return;

	x->Phiola_class = jni_global_ref(jni_class(PJC_PHIOLA));
	x->Phiola_lib_load = jni_sfunc(x->Phiola_class, "lib_load", "(" JNI_TSTR ")" JNI_TBOOL);

	x->Meta.cls = jni_global_ref(jni_class(PJC_META));
	x->Meta.init = jni_func(x->Meta.cls, "<init>", "()V");

	x->rec.RecInfo.cls = jni_global_ref(jni_class(PJC_RECINFO));
	x->rec.RecInfo.init = jni_func(x->rec.RecInfo.cls, "<init>", "()V");

	x->UtilNative_Files.cls = jni_global_ref(jni_class(PJC_UN_FILES));
	x->UtilNative_Files.init = jni_func(x->UtilNative_Files.cls, "<init>", "()V");

	FF_ASSERT(x->Phiola_class && x->Meta.cls && x->Phiola_lib_load);

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
	jni_global_unref(x->Callbacks.obj);
	jni_global_unref(x->QueueCallback.obj);
	jni_global_unref(x->Meta.cls);
	jni_global_unref(x->UtilNative_Files.cls);
	jni_global_unref(x->Phiola_class);

	char **it;
	FFSLICE_WALK(&x->storage_paths, it) {
		ffmem_free(*it);
	}
	ffvec_free(&x->storage_paths);

	ffmem_free(x->play.equalizer);
	ffmem_free(x->play.tee_filename);
	ffmem_free(x->rec.device_id);
	// ffmem_free(conv_track_info.error);
	ffvec_free_align(&x->convert.tracks);
	ffmem_free(x->convert.trash_dir_rel);
	ffmem_free(x);  x = NULL;
}

JNIEXPORT jstring JNICALL
Java_com_github_stsaz_phiola_Phiola_version(JNIEnv *env, jobject thiz)
{
	return jni_js_sz(x->core->version_str);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_setDebug(JNIEnv *env, jobject thiz, jboolean enable)
{
	x->debug = !!enable;
	x->core->conf.log_level = (x->debug) ? PHI_LOG_EXTRA : PHI_LOG_VERBOSE;
}

static void conf_apply(struct core_data *d)
{
	struct Config *c = (void*)d->param_str;

	ffmem_free(x->play.equalizer);
	x->play.equalizer = c->equalizer;

	qc_apply(); // Apply settings for the active playlist

	ffmem_free(c);
	ffmem_free(d);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_setConfig(JNIEnv *env, jobject thiz, jint flags, jobject conf)
{
	dbglog("%s: enter", __func__);

	struct Config *c = ffmem_new(struct Config);
	jni_obj_read(env, c, Config_map, conf, jni_class_obj(conf));

	const char *s = jni_sz_js(c->codepage);
	if (ffsz_eq(s, "win1251"))
		x->core->conf.code_page = FFUNICODE_WIN1251;
	else if (ffsz_eq(s, "win1252"))
		x->core->conf.code_page = FFUNICODE_WIN1252;
	jni_sz_free(s, c->codepage);

	x->play.remove_on_error = !!(c->queue_flags & QC_REMOVE_ON_ERROR);
	x->play.repeat_all = !!(c->queue_flags & QC_REPEAT);
	x->play.random = !!(c->queue_flags & QC_RANDOM);
	x->play.rg_normalizer = !!(c->queue_flags & QC_RG_NORM);
	x->play.auto_normalizer = !!(c->queue_flags & QC_AUTO_NORM);
	x->play.auto_seek_sec_percent = c->auto_seek;
	x->play.auto_until_sec_percent = c->auto_until;

	c->equalizer = (jstring)jni_sz_js_dup(env, c->equalizer);

	struct core_data *d = ffmem_new(struct core_data);
	d->param_str = (void*)c;
	core_task(d, conf_apply);

	dbglog("%s: exit", __func__);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_setCallbacks(JNIEnv *env, jobject thiz, jobject cb)
{
	dbglog("%s: enter", __func__);
	jni_if_read(env, (struct jni_if*)&x->Callbacks, Callbacks_map, cb);
	dbglog("%s: exit", __func__);
}

enum {
	TE_CLEAR = 1,
	TE_PRESERVE_DATE = 2,
};

/** {k=v}... <- {k, v, ...} */
static ffvec tags_from_java(JNIEnv *env, jobjectArray jtags)
{
	ffvec tags = {};
	uint n = jni_arr_len(jtags);
	ffvec_allocT(&tags, n / 2, ffstr);
	for (uint i = 0;  i + 1 < n;  i += 2) {
		jstring jk = jni_joa_i(jtags, i),  jval = jni_joa_i(jtags, i + 1);
		const char *k = jni_sz_js(jk),  *val = jni_sz_js(jval);

		ffstr *it = _ffvec_push(&tags, sizeof(ffstr));
		ffmem_zero_obj(it);
		size_t cap = 0;
		ffstr_growfmt(it, &cap, "%s=%s", k, val);

		jni_sz_free(k, jk);
		jni_local_unref(jk);
		jni_sz_free(val, jval);
		jni_local_unref(jval);
	}

	return tags;
}

JNIEXPORT jint JNICALL
Java_com_github_stsaz_phiola_Phiola_tagsEdit(JNIEnv *env, jobject thiz, jstring jfilename, jobjectArray jtags, jint flags)
{
	dbglog("%s: enter", __func__);
	int rc = 1;
	ffvec tags = tags_from_java(env, jtags);

	struct phi_tag_conf conf = {
		.filename = jni_sz_js(jfilename),
		.meta = *(ffslice*)&tags,
		.clear = !!(flags & TE_CLEAR),
		.preserve_date = !!(flags & TE_PRESERVE_DATE),
	};

	if (!x->tag_if)
		x->tag_if = x->core->mod("format.tag");
	rc = x->tag_if->edit(&conf);

	jni_sz_free(conf.filename, jfilename);
	jni_stra_free(&tags);
	dbglog("%s: exit", __func__);
	return rc;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *_jvm, void *reserved)
{
	jvm = _jvm;
	return JNI_VERSION_1_6;
}
