/** phiola: ICY server: bridge to netmill
2025, Simon Zolin */

static void nml_log(void *log_obj, uint level, const char *ctx, const char *id, const char *format, ...)
{
	level = nml_phi_log_level(level);

	va_list va;
	va_start(va, format);
	core->conf.logv(core->conf.log_obj, level, "audio-server", NULL, format, va);
	va_end(va);
}

static struct zzkevent* nmlcore_kev_new(void *boss)
{
	struct ausv *s = boss;
	return (struct zzkevent*)core->kev_alloc(s->worker);
}

static void nmlcore_kev_free(void *boss, struct zzkevent *kev)
{
	struct ausv *s = boss;
	core->kev_free(s->worker, (phi_kevent*)kev);
}

static int nmlcore_kq_attach(void *boss, ffsock sk, struct zzkevent *kev, void *obj)
{
	struct ausv *s = boss;
	kev->obj = obj;
	return core->kq_attach(s->worker, (phi_kevent*)kev, (fffd)sk, 0);
}

static void nmlcore_timer(void *boss, nml_timer *tmr, int interval_msec, fftimerqueue_func func, void *param)
{
	struct ausv *s = boss;
	core->timer(s->worker, (phi_timer*)tmr, interval_msec, func, param);
}

static void nmlcore_task(void *boss, nml_task *t, uint flags)
{
	struct ausv *s = boss;
	if (flags == 0)
		core->task(s->worker, (phi_task*)t, NULL, NULL);
	else
		core->task(s->worker, (phi_task*)t, t->handler, t->param);
}

static fftime nmlcore_date(void *boss, ffstr *dts)
{
	fftime t;
	fftime_now(&t);
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


static nml_wrk* nmlwrk_create(nml_core *core)
{
	*core = nmlcore;
	return (nml_wrk*)gs;
}

static void nmlwrk_free(nml_wrk *w)
{
}

static int nmlwrk_conf(nml_wrk *w, struct nml_wrk_conf *conf)
{
	return 0;
}

static int nmlwrk_run(nml_wrk *w)
{
	return 0;
}

static void nmlwrk_stop(nml_wrk *w)
{
}

static const nml_worker_if nmlwrk_if = {
	nmlwrk_create, nmlwrk_free,
	nmlwrk_conf,
	nmlwrk_run, nmlwrk_stop,
};


struct ausv_cl {
	size_t rpos;
	uint icy_meta_off;
	uint last_meta_uid;
};

static void ausv_client_paused(struct ausv *s, nml_http_sv_conn *hsc)
{
	*ffvec_pushT(&s->clients_paused, nml_http_sv_conn*) = hsc;
}

static void ausv_client_connected(struct ausv *s, nml_http_sv_conn *c)
{
	s->clients++;
}

static void ausv_client_closed(struct ausv *s, nml_http_sv_conn *c)
{
	// Remove this client from the list of suspended clients
	nml_http_sv_conn **hsc;
	FFSLICE_WALK(&s->clients_paused, hsc) {
		if (*hsc == c) {
			size_t i = hsc - (nml_http_sv_conn**)s->clients_paused.ptr;
			ffslice_rmswapT((ffslice*)&s->clients_paused, i, 1, void*);
			break;
		}
	}

	s->clients--;
}

/** Notify all suspended clients to continue their work. */
static void ausv_client_unpause(struct ausv *s)
{
	if (s->clients_paused.len) {
		nml_http_sv_conn **hsc;
		ffvec v = {};
		ffvec_add2T(&v, &s->clients_paused, void*);
		s->clients_paused.len = 0;
		FFSLICE_WALK(&v, hsc) {
			(*hsc)->conf->cl_wake(*hsc);
		}
		ffvec_free(&v);
	}
}

/** Process HTTP request headers */
static void phi_sv_req_headers(nml_http_sv_conn *c)
{
	const struct ausv *s = c->conf->opaque;
	struct ausv_cl *sc = c->proxy;

	ffstr h = HS_REQUEST_DATA(c, c->req.headers), name = {}, val = {};
	for (;;) {
		int r = http_hdr_parse(h, &name, &val);
		if (r <= 2)
			break;
		ffstr_shift(&h, r);

		if (ffstr_ieqz(&name, "Icy-MetaData")
			&& ffstr_eqz(&val, "1")) {
			c->resp.headers = s->resp_headers;
			sc->icy_meta_off = AUSV_CLIENT_BUF_SIZE_KB * 1024;
		}
	}
}

static int phi_sv_open(nml_http_sv_conn *c)
{
	struct ausv *s = c->conf->opaque;

	ffstr method = HS_REQUEST_DATA(c, c->req.method);
	if (!ffstr_eqz(&method, "GET")) {
		hs_response_err(c, HTTP_405_METHOD_NOT_ALLOWED);
		return NMLR_DONE;
	}

	ffstr path = HS_REQUEST_DATA(c, c->req.path);
	if (!ffstr_eqz(&path, "/")) {
		hs_response_err(c, HTTP_404_NOT_FOUND);
		return NMLR_DONE;
	}

	struct ausv_cl *sc = ffmem_new(struct ausv_cl);
	sc->icy_meta_off = ~0U;
	sc->last_meta_uid = s->meta_uid - 1;
	c->proxy = sc;

	ausv_client_connected(s, c);
	hs_response(c, HTTP_200_OK);
	c->req_no_chunked = 1;

	if (!s->ogg_opus)
		phi_sv_req_headers(c);

	ffvec_alloc(&c->file.buf, AUSV_CLIENT_BUF_SIZE_KB * 1024, 1);
	if (s->ogg_opus)
		ffvec_addstr(&c->file.buf, &s->meta);
	return NMLR_OPEN;
}

static void phi_sv_close(nml_http_sv_conn *c)
{
	struct ausv_cl *sc = c->proxy;
	struct ausv *s = c->conf->opaque;
	ausv_client_closed(s, c);
	ffvec_free(&c->file.buf);
	ffmem_free(sc);
}

/**
Return the max. size of the next audio data chunk */
static size_t phi_sv_icy_meta(nml_http_sv_conn *c, size_t n)
{
	struct ausv *s = c->conf->opaque;
	struct ausv_cl *sc = c->proxy;
	ffvec *buf = &c->file.buf;

	if (sc->icy_meta_off == ~0U)
		return n;

	if (sc->icy_meta_off == 0) {
		sc->icy_meta_off = AUSV_CLIENT_BUF_SIZE_KB * 1024;
		uint mu = ausv_icy_meta_read(s, sc->last_meta_uid, buf);
		if (sc->last_meta_uid != mu) {
			// Meta has been changed
			sc->last_meta_uid = mu;
		} else {
			ffmem_copy(buf->ptr, "\0", 1);
			buf->len = 1;
		}
	}

	return ffmin(n, sc->icy_meta_off);
}

static inline int ring_peek_at(ffring *r, size_t off, size_t n, ffstr *d1, ffstr *d2)
{
	size_t wtail = FFINT_READONCE(r->wtail);
	ffcpu_fence_acquire(); // read 'wtail' before data
	n = ffmin(n, wtail - off);
	size_t i = off & r->mask;
	ffstr_set(d1, r->data + i, n);
	ffstr_set(d2, r->data, 0);
	if (i + n > r->cap) {
		d1->len = r->cap - i;
		d2->len = i + n - r->cap;
	}
	return n;
}

static int phi_sv_process(nml_http_sv_conn *c)
{
	struct ausv_cl *sc = c->proxy;
	struct ausv *s = c->conf->opaque;
	ffvec *buf = &c->file.buf;

	size_t n = phi_sv_icy_meta(c, ffvec_unused(buf));

	size_t rtail = FFINT_READONCE(s->oring->rtail);
	if ((ssize_t)(sc->rpos - rtail) < 0)
		sc->rpos = rtail; // Catch up with the audio track

	ffstr d1, d2;
	n = ring_peek_at(s->oring, sc->rpos, n, &d1, &d2);
	if (!n) {
		ausv_client_paused(s, c);
		return NMLR_ASYNC;
	}
	ffmem_copy(buf->ptr + buf->len, d1.ptr, d1.len);
	ffmem_copy(buf->ptr + buf->len + d1.len, d2.ptr, d2.len);
	buf->len += n;
	sc->rpos += n;
	if (sc->icy_meta_off != ~0U)
		sc->icy_meta_off -= n;

	c->output = *(ffstr*)buf;
	buf->len = 0;
	return NMLR_FWD;
}

/** Prepares output data for a client */
static const nml_http_sv_component nml_http_sv_phi_bridge = {
	phi_sv_open, phi_sv_close, phi_sv_process,
	"phiola-sv-cl"
};


#include <http-server/request-receive.h>
#include <http-server/request.h>
#include <http-server/error.h>
#include <http-server/transfer.h>
#include <http-server/response.h>
#include <http-server/response-send.h>
#include <http-server/access-log.h>

static const nml_http_sv_component* sv_chain[] = {
	&nml_http_sv_receive,
	&nml_http_sv_request,
	&nml_http_sv_phi_bridge,
	&nml_http_sv_error,
	&nml_http_sv_transfer,
	&nml_http_sv_response,
	&nml_http_sv_send,
	&nml_http_sv_accesslog,
	NULL,
};

#ifndef PHI_HTTP_NO_SSL

#include <ssl/htsv-receive.h>
#include <ssl/htsv-handshake.h>
#include <ssl/htsv-send.h>
#include <ssl/htsv-request.h>
#include <ssl/htsv-response.h>

static const nml_http_sv_component* sv_ssl_chain[] = {
	&nml_htsv_ssl_recv,
	&nml_htsv_ssl_handshake,
	&nml_htsv_ssl_send,
	&nml_htsv_ssl_recv,
	&nml_htsv_ssl_req,
	&nml_http_sv_request,
	&nml_http_sv_phi_bridge,
	&nml_http_sv_error,
	&nml_http_sv_transfer,
	&nml_http_sv_response,
	&nml_htsv_ssl_resp,
	&nml_htsv_ssl_send,
	&nml_http_sv_accesslog,
	NULL,
};

static struct nml_ssl_ctx* sv_ssl_prepare(const struct ausv *s, ffstr cert_data)
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
	sc->log_obj = (void*)s;

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

int ausv_create(struct ausv *s, const struct phi_asv_conf *ac, phi_track *t)
{
	s->sv = nml_http_server_interface.create();
	struct nml_http_server_conf sc;
	nml_http_server_interface.conf(NULL, &sc);
	sc.response.server_name = FFSTR_Z("phiola/2");
	sc.opaque = s;
	sc.server.wif = &nmlwrk_if;
	sc.server.lsif = &nml_tcp_listener_interface;
	s->addr.port = ac->port;
	sc.server.listen_addresses = &s->addr;
	if (ac->max_clients)
		sc.server.max_connections = ac->max_clients;
	sc.chain = sv_chain;

#ifndef PHI_HTTP_NO_SSL
	if (ac->ssl_cert.len) {
		if (!sv_ssl_ctx
			&& !(sv_ssl_ctx = sv_ssl_prepare(s, ac->ssl_cert))) {
			errlog(t, "can't initialize SSL context");
			return -1;
		}
		sc.ssl_ctx = sv_ssl_ctx;
		sc.chain = sv_ssl_chain;
	}
#endif

	sc.log_level = phi_nml_log_level(core->conf.log_level);
	if (!sc.log_level)
		sc.log_level = NML_LOG_VERBOSE;
	sc.log = nml_log;
	sc.log_obj = s;

	if (nml_http_server_interface.conf(s->sv, &sc))
		return -1;

	nml_http_server_interface.run(s->sv);
	return 0;
}
