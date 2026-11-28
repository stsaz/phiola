/** phiola: HTTP remote control server: netmill stuff
2026, Simon Zolin */

#include <netmill-http.h>

static void nml_log(void *log_obj, uint level, const char *ctx, const char *id, const char *format, ...)
{
	level = nml_phi_log_level(level);

	va_list va;
	va_start(va, format);
	core->conf.logv(core->conf.log_obj, level, "http-ctl", NULL, format, va);
	va_end(va);
}

static struct zzkevent* nmlcore_kev_new(void *boss)
{
	struct http_ctl_sv *s = boss;
	return (struct zzkevent*)core->kev_alloc(s->worker);
}

static void nmlcore_kev_free(void *boss, struct zzkevent *kev)
{
	struct http_ctl_sv *s = boss;
	core->kev_free(s->worker, (phi_kevent*)kev);
}

static int nmlcore_kq_attach(void *boss, ffsock sk, struct zzkevent *kev, void *obj)
{
	struct http_ctl_sv *s = boss;
	kev->obj = obj;
	return core->kq_attach(s->worker, (phi_kevent*)kev, (fffd)sk, 0);
}

static void nmlcore_timer(void *boss, nml_timer *tmr, int interval_msec, fftimerqueue_func func, void *param)
{
	struct http_ctl_sv *s = boss;
	core->timer(s->worker, (phi_timer*)tmr, interval_msec, func, param);
}

static void nmlcore_task(void *boss, nml_task *t, uint flags)
{
	struct http_ctl_sv *s = boss;
	if (flags == 0)
		core->task(s->worker, (phi_task*)t, NULL, NULL);
	else
		core->task(s->worker, (phi_task*)t, t->handler, t->param);
}

static fftime nmlcore_date(void *boss, ffstr *dts)
{
	struct http_ctl_sv *s = boss;
	fftime t;
	fftime_now(&t);

	if (dts) {
		fftime t1 = t;
		t1.sec += FFTIME_1970_SECONDS;
		ffdatetime dt;
		fftime_split1(&dt, &t1);
		fftime_tostr1(&dt, s->date_buf, sizeof(s->date_buf), FFTIME_DATE_YMD | FFTIME_HMS_MSEC);
		s->date_buf[10] = 'T';
		ffstr_set(dts, s->date_buf, sizeof(s->date_buf)-1);
	}
	return t;
}

/** Bridge between netmill and phiola Core */
static const struct nml_core nmlcore = {
	.kev_new = nmlcore_kev_new,
	.kev_free = nmlcore_kev_free,
	.kq_attach = nmlcore_kq_attach,
	.timer = nmlcore_timer,
	.task = nmlcore_task,
	.date = nmlcore_date,
};


#include <http-server/virtspace.h>
#include <http-server/index.h>
#include <http-server/file.h>

extern const nml_http_sv_component
	nml_http_sv_receive,
	nml_http_sv_request,
	nml_http_sv_virtspace,
	nml_http_sv_index,
	nml_http_sv_file,
	nml_http_sv_error,
	nml_http_sv_transfer,
	nml_http_sv_response,
	nml_http_sv_send,
	nml_http_sv_accesslog,
	nml_http_sv_keepalive;

static const nml_http_sv_component* hcsv_chain[] = {
	&nml_http_sv_receive,
	&nml_http_sv_request,
	&nml_http_sv_virtspace,
	&nml_http_sv_index,
	&nml_http_sv_file,
	&nml_http_sv_error,
	&nml_http_sv_transfer,
	&nml_http_sv_response,
	&nml_http_sv_send,
	&nml_http_sv_accesslog,
	&nml_http_sv_keepalive,
	NULL,
};

#ifndef PHI_HTTP_NO_SSL

extern const nml_http_sv_component
	nml_htsv_ssl_recv,
	nml_htsv_ssl_handshake,
	nml_htsv_ssl_send,
	nml_htsv_ssl_recv,
	nml_htsv_ssl_req,
	nml_htsv_ssl_resp;

static const nml_http_sv_component* hcsv_ssl_chain[] = {
	&nml_htsv_ssl_recv,
	&nml_htsv_ssl_handshake,
	&nml_htsv_ssl_send,
	&nml_htsv_ssl_recv,
	&nml_htsv_ssl_req,
	&nml_http_sv_request,
	&nml_http_sv_virtspace,
	&nml_http_sv_index,
	&nml_http_sv_file,
	&nml_http_sv_error,
	&nml_http_sv_transfer,
	&nml_http_sv_response,
	&nml_htsv_ssl_resp,
	&nml_htsv_ssl_send,
	&nml_http_sv_accesslog,
	&nml_http_sv_keepalive,
	NULL,
};

static struct nml_ssl_ctx* hcsv_ssl_prepare(struct http_ctl_sv *s, ffstr cert_data)
{
	struct nml_ssl_ctx *sc = ffmem_new(struct nml_ssl_ctx);
	struct ffssl_ctx_conf *scc = ffmem_new(struct ffssl_ctx_conf);
	sc->ctx_conf = scc;

	scc->cert_data = cert_data;
	scc->pkey_data = cert_data;
	scc->use_server_cipher = 1;

	sc->log_level = phi_nml_log_level(core->conf.log_level);
	if (!sc->log_level)
		sc->log_level = NML_LOG_VERBOSE;
	sc->log = nml_log;
	sc->log_obj = s;

	if (!cert_data.len || nml_ssl_init(sc))
		goto err;

	ffstr_free(&cert_data);
	return sc;

err:
	ffstr_free(&cert_data);
	ffmem_free(scc);
	ffmem_free(sc);
	return NULL;
}

#endif
