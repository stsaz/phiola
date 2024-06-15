/** phiola/Android: playback functionality
2024, Simon Zolin */

#include <util/aformat.h>

static void* play_grd_open(phi_track *t)
{
	x->play.trk = t;
	x->play.seek_msec = -1;
	x->play.paused = 0;
	return (void*)1;
}

static void play_grd_close(void *f, phi_track *t)
{
	x->play.trk = NULL;

	if (t->chain_flags & PHI_FFINISHED) {
		if (t->error == PHI_E_NOSRC
			|| t->error == PHI_E_UNKIFMT) {
			if (x->play.remove_on_error)
				x->queue.remove(t->qent);
		}
	}
}

static int play_grd_process(void *f, phi_track *t)
{
	return PHI_DONE;
}

static const phi_filter phi_play_guard = {
	play_grd_open, play_grd_close, play_grd_process,
	"play-guard"
};


static void* play_ui_open(phi_track *t)
{
	x->play.opened = 0;
	return (void*)1;
}

enum {
	PCS_STOP = 1,
	PCS_AUTOSTOP = 2,
};

static void play_ui_close(void *f, phi_track *t)
{
	JNIEnv *env;
	int r = jni_vm_attach(jvm, &env);
	if (r != 0) {
		errlog("jni_vm_attach: %d", r);
		return;
	}

	int status = 0;

	if (x->play.auto_stop_timer_expired) {
		x->play.auto_stop_timer_expired = 0;
		t->chain_flags |= PHI_FSTOP;
		status |= PCS_AUTOSTOP;
	}

	status |= (t->chain_flags & PHI_FSTOP) ? PCS_STOP : 0;

	trk_dbglog(t, "PlayObserver_on_close");
	jni_call_void(x->play.obj_PlayObserver, x->play.PlayObserver_on_close, status);
	jni_vm_detach(jvm);

}

#define pcm_samples_to_time_msec(samples, rate)   ((uint64)(samples) * 1000 / (rate))

static void meta_fill(JNIEnv *env, jobject jmeta, const phi_track *t)
{
	jni_obj_sz_set(env, jmeta, jni_field_str(x->Phiola_Meta, "url"), t->conf.ifile.name);

	struct phi_queue_entry *qe = (struct phi_queue_entry*)t->qent;
	const ffvec *meta = &qe->conf.meta;

	uint i = 0;
	ffstr k, v;
	while (x->metaif.list(meta, &i, &k, &v, PHI_META_UNIQUE | PHI_META_PRIVATE)) {
		if (ffstr_eqz(&k, "artist"))
			jni_obj_sz_set(env, jmeta, jni_field_str(x->Phiola_Meta, "artist"), v.ptr);

		else if (ffstr_eqz(&k, "title"))
			jni_obj_sz_set(env, jmeta, jni_field_str(x->Phiola_Meta, "title"), v.ptr);

		else if (ffstr_eqz(&k, "album"))
			jni_obj_sz_set(env, jmeta, jni_field_str(x->Phiola_Meta, "album"), v.ptr);

		else if (ffstr_eqz(&k, "date"))
			jni_obj_sz_set(env, jmeta, jni_field_str(x->Phiola_Meta, "date"), v.ptr);

		else if (ffstr_eqz(&k, "_phi_info"))
			jni_obj_sz_set(env, jmeta, jni_field_str(x->Phiola_Meta, "info"), v.ptr);
	}

	if (t->audio.format.rate) {
		uint64 duration_msec = pcm_samples_to_time_msec(t->audio.total, t->audio.format.rate);
		jni_obj_long_set(jmeta, jni_field_long(x->Phiola_Meta, "length_msec"), duration_msec);
		qe->length_msec = duration_msec;
	}

	jni_obj_int_set(jmeta, jni_field_int(x->Phiola_Meta, "queue_pos"), x->queue.index(qe));
}

static int handle_seek(phi_track *t)
{
	if (x->play.seek_msec != -1) {
		t->audio.seek = x->play.seek_msec;
		x->play.seek_msec = -1;
		return PHI_MORE; // new seek request

	} else if (!(t->chain_flags & PHI_FFWD)) {
		return PHI_MORE; // going back without seeking

	} else if (t->data_in.len == 0 && !(t->chain_flags & PHI_FFIRST)) {
		return PHI_MORE; // waiting for audio data

	} else if (t->audio.seek != -1 && !t->audio.seek_req) {
		trk_dbglog(t, "seek: done");
		t->audio.seek = ~0ULL; // prev. seek is complete
	}
	return 0;
}

static void auto_skip(phi_track *t)
{
	int as = x->play.auto_seek_sec_percent, au = x->play.auto_until_sec_percent;
	if (!(as || au))
		return;

	uint64 dur_msec = pcm_samples_to_time_msec(t->audio.total, t->audio.format.rate);

	x->play.seek_msec = (as > 0) ? (uint64)as * 1000
		: dur_msec * -as / 100;
	t->conf.until_msec = (au > 0) ? dur_msec - au * 1000
		: dur_msec - (dur_msec * -au / 100);
}

static int play_ui_process(void *f, phi_track *t)
{
	uint64 pos_msec = 0;
	uint pos_sec = 0;
	if (t->audio.format.rate) {
		if (t->audio.pos != ~0ULL) {
			pos_msec = pcm_samples_to_time_msec(t->audio.pos, t->audio.format.rate);
			pos_sec = t->audio.pos / t->audio.format.rate;
		}
	}

	if (!x->play.opened) {
		x->play.opened = 1;

		struct phi_queue_entry *qe = (struct phi_queue_entry*)t->qent;

		char buf[100];
		ffsz_format(buf, sizeof(buf), "%u kbps, %s, %u Hz, %s, %s"
			, (t->audio.bitrate + 500) / 1000
			, t->audio.decoder
			, t->audio.format.rate
			, pcm_channelstr(t->audio.format.channels)
			, phi_af_name(t->audio.format.format));
		x->metaif.set(&t->meta, FFSTR_Z("_phi_info"), FFSTR_Z(buf), 0);

		// We need to display the currently active track's meta data before `queue` does this on track close
		fflock_lock((fflock*)&qe->lock); // UI thread may read or write `conf.meta` at this moment
		ffvec meta_old = qe->conf.meta;
		qe->conf.meta = t->meta;
		fflock_unlock((fflock*)&qe->lock);

		x->metaif.destroy(&meta_old);
		ffvec_null(&t->meta);

		JNIEnv *env;
		int r = jni_vm_attach(jvm, &env);
		if (r != 0) {
			errlog("jni_vm_attach: %d", r);
			return PHI_ERR;
		}

		jobject jmeta = jni_obj_new(x->Phiola_Meta, x->Phiola_Meta_init);
		meta_fill(env, jmeta, t);
		trk_dbglog(t, "PlayObserver_on_create");
		jni_call_void(x->play.obj_PlayObserver, x->play.PlayObserver_on_create, jmeta);
		jni_vm_detach(jvm);

		auto_skip(t);
	}

	if (handle_seek(t))
		return PHI_MORE;

	if (pos_sec != x->play.pos_prev_sec) {
		x->play.pos_prev_sec = pos_sec;

		if (t->conf.until_msec && pos_sec * 1000 >= t->conf.until_msec) {
			trk_dbglog(t, "reached position %U", t->conf.until_msec / 1000);
			return PHI_LASTOUT;
		}

		JNIEnv *env;
		int r = jni_vm_attach(jvm, &env);
		if (r != 0) {
			errlog("jni_vm_attach: %d", r);
			return PHI_ERR;
		}
		trk_dbglog(t, "PlayObserver_on_update");
		jni_call_void(x->play.obj_PlayObserver, x->play.PlayObserver_on_update, (jlong)pos_msec);
		jni_vm_detach(jvm);
	}

	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

static const phi_filter phi_play_ui = {
	play_ui_open, play_ui_close, play_ui_process,
	"play-ui"
};


JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_playObserverSet(JNIEnv *env, jobject thiz, jobject jo, jint flags)
{
	dbglog("%s: enter", __func__);
	jclass jc = jni_class_obj(jo);
	x->play.PlayObserver_on_create = jni_func(jc, "on_create", "(" PJT_META ")" JNI_TVOID);
	x->play.PlayObserver_on_close = jni_func(jc, "on_close", "(" JNI_TINT ")" JNI_TVOID);
	x->play.PlayObserver_on_update = jni_func(jc, "on_update", "(" JNI_TLONG ")" JNI_TVOID);
	x->play.obj_PlayObserver = jni_global_ref(jo);
	dbglog("%s: exit", __func__);
}

static void play_pause_resume(struct core_data *d)
{
	phi_track *t = x->play.trk;
	if (!t)
		goto end;

	if (x->play.paused) {
		x->play.paused = 0;
		uint p = t->oaudio.pause;
		t->oaudio.pause = 0;
		trk_dbglog(t, "unpausing");
		if (!p)
			x->core->track->wake(t);
		goto end;
	}

	t->oaudio.pause = 1;
	x->play.paused = 1;

	if (t->oaudio.adev_ctx)
		t->oaudio.adev_stop(t->oaudio.adev_ctx);

	trk_dbglog(t, "pausing");

end:
	ffmem_free(d);
}

static void play_auto_stop_timer(void *param)
{
	x->play.auto_stop_timer_expired = 1;
}

static void play_auto_stop(struct core_data *d)
{
	int interval_msec = d->param_int;
	x->core->timer(0, &x->play.auto_stop_timer, -interval_msec, play_auto_stop_timer, NULL);
	ffmem_free(d);
}

static void play_seek(struct core_data *d)
{
	phi_track *t = x->play.trk;
	if (!t)
		goto end;

	x->play.seek_msec = d->param_int;
	t->audio.seek_req = 1;
	t->oaudio.clear = 1;
	if (t->oaudio.adev_ctx)
		t->oaudio.adev_stop(t->oaudio.adev_ctx);
	trk_dbglog(t, "seek: %U", x->play.seek_msec);

end:
	ffmem_free(d);
}

enum {
	PC_PAUSE_TOGGLE = 1,
	PC_STOP = 2,
	PC_AUTO_SKIP_HEAD = 3,
	PC_AUTO_SKIP_TAIL = 4,
	PC_AUTO_STOP = 5,
	PC_SEEK = 6,
};

JNIEXPORT void JNICALL
Java_com_github_stsaz_phiola_Phiola_playCmd(JNIEnv *env, jobject thiz, jint cmd, jlong val)
{
	dbglog("%s: enter", __func__);
	struct core_data *d;
	switch (cmd) {
	case PC_PAUSE_TOGGLE:
		d = ffmem_new(struct core_data);
		core_task(d, play_pause_resume);
		break;

	case PC_STOP:
		x->core->track->stop(x->play.trk);  break;

	case PC_SEEK:
		d = ffmem_new(struct core_data);
		d->param_int = val;
		core_task(d, play_seek);
		break;

	case PC_AUTO_SKIP_HEAD:
		x->play.auto_seek_sec_percent = val;  break;

	case PC_AUTO_SKIP_TAIL:
		x->play.auto_until_sec_percent = val;  break;

	case PC_AUTO_STOP:
		d = ffmem_new(struct core_data);
		d->param_int = val;
		core_task(d, play_auto_stop);
		break;
	}
	dbglog("%s: exit", __func__);
}
