/** phiola/Android: playback-record functionality
2026, Simon Zolin */

#include <avpack/decl.h>

static void tee_out_nfy(int code, const char *fn)
{
	JNIEnv *env;
	int r = jni_vm_attach(jvm, &env);
	if (r) {
		errlog("jni_vm_attach: %d", r);
		return;
	}

	jstring js = (fn) ? jni_js_sz(fn) : NULL;
	jni_call_void(x->Callbacks.obj, x->Callbacks.recording, 1, code, js);

	jni_vm_detach(jvm);
}

static void* tee_out_open(phi_track *t)
{
	return (void*)1;
}

static void tee_out_close(void *f, phi_track *t)
{
	const char *fn = (t->output.name) ? t->output.name : t->conf.ofile.name;
	tee_out_nfy(t->error, fn);
}

static int tee_out_process(void *f, phi_track *t)
{
	return PHI_DONE;
}

static const phi_filter tee_out_guard = {
	tee_out_open, tee_out_close, tee_out_process,
	"and-tee-out",
};

static const char* rec_ext(uint fmt)
{
	switch (fmt) {
	case AVPKF_MP3: return "mp3";
	case AVPKF_AAC: return "aac";
	}
	return NULL;
}

/** Start/stop recording from radio using 'tee' module */
static void play_rec(struct core_data *d)
{
	phi_track *t = x->play.trk;
	char *oname = d->param_str;

	if (!t) {
		tee_out_nfy(PHI_E_NOSRC, NULL);
		goto end;
	}

	if (!t->tee_active) {
		if (!oname)
			goto end; // duplicate 'rec-stop' request

		const char *ext = rec_ext(t->input.format);
		if (!ext) {
			tee_out_nfy(PHI_E_UNKIFMT, NULL);
			goto end;
		}
		ffmem_free(x->play.tee_filename);
		x->play.tee_filename = ffsz_allocfmt("%s.%s"
			, oname, ext);
		t->conf.tee = x->play.tee_filename;
	}
	t->tee_active = !t->tee_active;

end:
	ffmem_free(oname);
	ffmem_free(d);
}

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_playRecord(JNIEnv *env, jobject thiz, jstring oname)
{
	dbglog("%s: enter", __func__);
	struct core_data *d = ffmem_new(struct core_data);
	d->param_str = jni_sz_js_dup(env, oname);
	core_task(d, play_rec);
	dbglog("%s: exit", __func__);
}
