/** JNI wrapper
2022, Simon Zolin */

/*
jni_attach jni_detach
jni_local_unref
jni_global_ref jni_global_unref
Meta:
	jni_class jni_class_obj
	jni_field
String:
	jni_js_sz
	jni_sz_js jni_sz_free
Arrays:
	jni_arr_len
	jni_bytes_jba jni_bytes_free
	jni_joa
	jni_joa_i jni_joa_i_set
	jni_jsa_sza
	jni_str_jba
Object:
	jni_obj_jo jni_obj_jo_set
	jni_obj_sz_set jni_obj_sz_setf
	jni_obj_long jni_obj_long_set
	jni_obj_int
	jni_obj_bool
Functions:
	jni_func jni_call_void
	jni_sfunc jni_scall_void
Notes:
	sz	char*
	js	jstring
	jo	jobject
	joa	jobjectArray
	jba	jobjectArray[jbyte]
	jsa	jobjectArray[String]
*/

#pragma once
#include <jni.h>

#define JNI_CSTR "java/lang/String"

#define JNI_TSTR "Ljava/lang/String;"
#define JNI_TOBJ "Ljava/lang/Object;"
#define JNI_TINT "I"
#define JNI_TLONG "J"
#define JNI_TBOOL "Z"
#define JNI_TARR "["

#define jni_attach(jvm, envp) \
	(*jvm)->AttachCurrentThread(jvm, envp, NULL)

#define jni_detach(jvm) \
	(*jvm)->DetachCurrentThread(jvm)

#define jni_local_unref(jobj) \
	(*env)->DeleteLocalRef(env, jobj)

#define jni_global_ref(jobj) \
	(*env)->NewGlobalRef(env, jobj)

#define jni_global_unref(jobj) \
	(*env)->DeleteGlobalRef(env, jobj)

/** jclass = class(JNI_C...) */
#define jni_class(C) \
	(*env)->FindClass(env, C)

/** jclass = typeof(obj) */
#define jni_class_obj(jobj) \
	(*env)->GetObjectClass(env, jobj)

/** jfieldID = jclass::field
T: JNI_T... */
#define jni_field(jclazz, name, T) \
	(*env)->GetFieldID(env, jclazz, name, T)


/** jstring = char* */
#define jni_js_sz(sz) \
	(*env)->NewStringUTF(env, sz)

/** char* = jstring */
#define jni_sz_js(js) \
	(*env)->GetStringUTFChars(env, js, NULL)

#define jni_sz_free(sz, js) \
do { \
	if (js != NULL) \
		(*env)->ReleaseStringUTFChars(env, js, sz); \
} while(0)


#define jni_arr_len(ja) \
	(*env)->GetArrayLength(env, ja)

/** ffbyte* = byte[] */
#define jni_bytes_jba(jab) \
	(*env)->GetByteArrayElements(env, jab, NULL)

#define jni_bytes_free(ptr, jab) \
	(*env)->ReleaseByteArrayElements(env, jab, (jbyte*)(ptr), 0)

/** jobjectArray = new */
#define jni_joa(cap, jclazz) \
	(*env)->NewObjectArray(env, cap, jclazz, NULL)

/** jobject = array[i] */
#define jni_joa_i(ja, i) \
	(*env)->GetObjectArrayElement(env, ja, i)

/** array[i] = jobject */
#define jni_joa_i_set(ja, i, val) \
	(*env)->SetObjectArrayElement(env, ja, i, val)

/** ffstr = byte[]
Free with jni_bytes_free(s.ptr, jba) */
static inline ffstr jni_str_jba(JNIEnv *env, jbyteArray *jba)
{
	ffstr s = FFSTR_INITN(jni_bytes_jba(jba), jni_arr_len(jba));
	return s;
}

/** String[] = char*[] */
static inline jobjectArray jni_jsa_sza(JNIEnv *env, char **asz, ffsize n)
{
	jclass cstr = jni_class(JNI_CSTR);
	jobjectArray jas = jni_joa(n, cstr);
	for (ffsize i = 0;  i != n;  i++) {
		char *s = asz[i];
		jstring js = jni_js_sz(s);
		jni_joa_i_set(jas, i, js);
		jni_local_unref(js);
	}
	return jas;
}


/** obj.object = VAL */
#define	jni_obj_jo_set(jobj, jfield, val) \
	(*env)->SetObjectField(env, jobj, jfield, val)

/** object = obj.object */
#define jni_obj_jo(jobj, jfield) \
	(*env)->GetObjectField(env, jobj, jfield)

/** obj.string = sz */
static inline void jni_obj_sz_set(JNIEnv *env, jobject jo, jfieldID jf, const char *sz)
{
	jstring js = jni_js_sz(sz);
	jni_obj_jo_set(jo, jf, js);
	jni_local_unref(js);
}

static inline void jni_obj_sz_setf(JNIEnv *env, jobject jo, jfieldID jf, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	char *sz = ffsz_allocfmtv(fmt, va);
	va_end(va);
	jni_obj_sz_set(env, jo, jf, sz);
	ffmem_free(sz);
}

/** long = obj.long */
#define jni_obj_long(jobj, jfield) \
	(*env)->GetLongField(env, jobj, jfield)

/** obj.long = VAL */
#define	jni_obj_long_set(jobj, jfield, val) \
	(*env)->SetLongField(env, jobj, jfield, val)

/** int = obj.int */
#define jni_obj_int(jobj, jfield) \
	(*env)->GetIntField(env, jobj, jfield)

/** bool = obj.bool */
#define jni_obj_bool(jobj, jfield) \
	(*env)->GetBooleanField(env, jobj, jfield)


/** jmethodID of a class function

class C {
	T name(P1, P2, ...) {}
	// signature template: (P1P2...)T
} */
#define jni_func(jclazz, name, sig) \
	(*env)->GetMethodID(env, jclazz, name, sig)

/** jmethodID of a class static function

class C {
	static T name() {}
} */
#define jni_sfunc(jclazz, name, sig) \
	(*env)->GetStaticMethodID(env, jclazz, name, sig)

#define jni_call_void(jobj, jfunc, ...) \
	(*env)->CallVoidMethod(env, jobj, jfunc, #__VA_ARGS__)

#define jni_scall_void(jobj, jfunc, ...) \
	(*env)->CallVoidMethod(env, jobj, jfunc, #__VA_ARGS__)
