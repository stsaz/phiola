/** phiola: HTTP client
2023, Simon Zolin */

#include <track.h>
#include <netmill.h>
#include <util/http1.h>
#include <util/kq.h>
#include <util/util.h>
#include <net/http-bridge.h>
#include <ffsys/globals.h>

const phi_core *core;
extern const phi_filter phi_icy;
#ifndef PHI_HTTP_NO_SSL
static struct nml_ssl_ctx *phi_ssl_ctx;
#endif
#define errlog(t, ...)  phi_errlog(core, "http-client", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "http-client", t, __VA_ARGS__)

struct httpcl {
	nml_http_client *cl;
	struct nml_http_client_conf conf;
	phi_track *trk;
	ffvec path;

	ffstr data;
	uint state; // enum ST
	uint fstate;
	uint done :1;
	uint icy :1;
};

enum ST {
	ST_PROCESSING,
	ST_WAIT,
	ST_DATA,
	ST_ERR,
};

static void nml_log(void *log_obj, uint level, const char *ctx, const char *id, const char *format, ...)
{
	struct httpcl *h = log_obj;
	static const uint levels[] = {
		/*NML_LOG_SYSFATAL*/PHI_LOG_ERR | PHI_LOG_SYS,
		/*NML_LOG_SYSERR*/	PHI_LOG_ERR | PHI_LOG_SYS,
		/*NML_LOG_ERR*/		PHI_LOG_ERR,
		/*NML_LOG_SYSWARN*/	PHI_LOG_WARN | PHI_LOG_SYS,
		/*NML_LOG_WARN*/	PHI_LOG_WARN,
		/*NML_LOG_INFO*/	PHI_LOG_INFO,
		/*NML_LOG_VERBOSE*/	PHI_LOG_VERBOSE,
		/*NML_LOG_DEBUG*/	PHI_LOG_DEBUG,
		/*NML_LOG_EXTRA*/	PHI_LOG_EXTRA,
	};
	level = levels[level];

	va_list va;
	va_start(va, format);
	core->conf.logv(core->conf.log_obj, level, NULL, h->trk, format, va);
	va_end(va);
}

static struct zzkevent* nmlcore_kev_new(void *boss)
{
	struct httpcl *h = boss;
	return (struct zzkevent*)core->kev_alloc(h->trk->worker);
}

static void nmlcore_kev_free(void *boss, struct zzkevent *kev)
{
	struct httpcl *h = boss;
	core->kev_free(h->trk->worker, (phi_kevent*)kev);
}

static int nmlcore_kq_attach(void *boss, ffsock sk, struct zzkevent *kev, void *obj)
{
	struct httpcl *h = boss;
	kev->obj = obj;
	return core->kq_attach(h->trk->worker, (phi_kevent*)kev, (fffd)sk, 0);
}

static void nmlcore_timer(void *boss, nml_timer *tmr, int interval_msec, fftimerqueue_func func, void *param)
{
	struct httpcl *h = boss;
	core->timer(h->trk->worker, (phi_timer*)tmr, interval_msec, func, param);
}

static void nmlcore_task(void *boss, nml_task *t, uint flags)
{
	struct httpcl *h = boss;
	if (flags == 0)
		core->task(h->trk->worker, (phi_task*)t, NULL, NULL);
	else
		core->task(h->trk->worker, (phi_task*)t, t->handler, t->param);
}

static fftime nmlcore_date(void *boss, ffstr *dts)
{
	fftime t;
	fftime_now(&t);
	return t;
}

static const struct nml_core nmlcore = {
	.kev_new = nmlcore_kev_new,
	.kev_free = nmlcore_kev_free,
	.kq_attach = nmlcore_kq_attach,
	.timer = nmlcore_timer,
	.task = nmlcore_task,
	.date = nmlcore_date,
};

int phi_hc_resp(void *ctx, struct phi_http_data *d)
{
	struct httpcl *h = ctx;

	if (d->code != 200) {
		errlog(NULL, "resource unavailable: %S", &d->status);
		return NMLR_ERR;
	}

	static const struct map_sz_vptr ct_ext[] = {
		{ "application/ogg",	"ogg" },
		{ "audio/aac",	"aac" },
		{ "audio/aacp",	"aac" },
		{ "audio/mpeg",	"mp3" },
		{ "audio/ogg",	"ogg" },
	};
	h->trk->data_type = map_sz_vptr_findstr(ct_ext, FF_COUNT(ct_ext), d->ct); // help format.detector in case it didn't detect format

	h->trk->icy_meta_interval = d->icy_meta_interval;
	h->icy = !!d->icy_meta_interval;
	h->trk->meta_changed = !d->icy_meta_interval;

	return NMLR_OPEN;
}

int phi_hc_data(void *ctx, ffstr data, uint flags)
{
	struct httpcl *h = ctx;

	switch (h->fstate) {
	case 0:
		h->data = data;
		h->done = !!(flags & 1);
		if (FF_SWAP(&h->state, ST_DATA) == ST_WAIT) {
			core->track->wake(h->trk);
			h->fstate = 1;
		}
		return NMLR_ASYNC;

	case 1:
		h->fstate = 0;
		return NMLR_BACK;
	}
	return NMLR_ERR;
}

static void on_complete(void *param)
{
	struct httpcl *h = param;
	if (FF_SWAP(&h->state, ST_ERR) == ST_WAIT)
		core->track->wake(h->trk);
}

extern const nml_http_cl_component
	*hc_chain[],
	*hc_ssl_chain[];

#ifndef PHI_HTTP_NO_SSL
#include <util/ssl.h>
static struct nml_ssl_ctx* ssl_prepare(struct nml_http_client_conf *c)
{
	struct nml_ssl_ctx *sc = ffmem_new(struct nml_ssl_ctx);
	struct ffssl_ctx_conf *scc = ffmem_new(struct ffssl_ctx_conf);
	sc->ctx_conf = scc;

	char *cert_file = NULL;
	ffstr cert_data = {};
#ifdef FF_ANDROID
	cert_data = core->conf.resource_load("http-client.pem");
	scc->cert_data = cert_data;
	scc->pkey_data = cert_data;
#else
	cert_file = ffsz_allocfmt("%S/mod/http-client.pem", &core->conf.root);
	scc->cert_file = cert_file;
	scc->pkey_file = cert_file;
#endif

	sc->log_level = c->log_level;
	sc->log_obj = c->log_obj;
	sc->log = c->log;

	if (nml_ssl_init(sc)) {
		ffmem_free(scc);
		ffmem_free(sc);
		sc = NULL;
	}

	ffstr_free(&cert_data);
	ffmem_free(cert_file);
	return sc;
}
#endif

static int conf_prepare(struct httpcl *h, struct nml_http_client_conf *c, phi_track *t)
{
	nml_http_client_conf(NULL, c);
	c->opaque = h;
	if (core->conf.log_level >= PHI_LOG_EXTRA)
		c->log_level = NML_LOG_EXTRA;
	else if (core->conf.log_level >= PHI_LOG_DEBUG)
		c->log_level = NML_LOG_DEBUG;
	c->log = nml_log;
	c->log_obj = h;

	c->wake = on_complete;
	c->wake_param = h;

	c->core = nmlcore;
	c->boss = h;

	struct httpurl_parts p = {};
	httpurl_split(&p, FFSTR_Z(h->trk->conf.ifile.name));
	c->host = p.host;
	c->host.len += p.port.len;
	if (!p.port.len)
		c->server_port = 80;
	ffvec_alloc(&h->path, p.path.len * 3, 1);
	h->path.len = httpurl_escape(h->path.ptr, h->path.cap, p.path);
	c->path = *(ffstr*)&h->path;

	c->chain = hc_chain;

	if (ffstr_ieqz(&p.scheme, "https://")) {
#ifndef PHI_HTTP_NO_SSL
		if (!phi_ssl_ctx
			&& !(phi_ssl_ctx = ssl_prepare(c))) {
			errlog(h->trk, "can't initialize SSL context");
			return -1;
		}
		c->ssl_ctx = phi_ssl_ctx;
		c->chain = hc_ssl_chain;
		if (!p.port.len)
			c->server_port = 443;

#else
		errlog(h->trk, "SSL support is disabled");
		return -1;
#endif
	}

	ffsize headers_cap = 0;
	ffstr_growaddz(&c->headers, &headers_cap, "User-Agent: phiola/2\r\n");

	if (!t->conf.ifile.no_meta)
		ffstr_growaddz(&c->headers, &headers_cap, "Icy-MetaData: 1\r\n");

	if (t->conf.ifile.connect_timeout_sec)
		c->connect_timeout_msec = t->conf.ifile.connect_timeout_sec * 1000;
	if (t->conf.ifile.recv_timeout_sec)
		c->receive.timeout_msec = t->conf.ifile.recv_timeout_sec * 1000;
	c->max_redirect = 10;
	return 0;
}

static void* httpcl_open(phi_track *t)
{
	struct httpcl *h = phi_track_allocT(t, struct httpcl);
	h->trk = t;
	h->cl = nml_http_client_create();

	struct nml_http_client_conf *c = &h->conf;
	if (conf_prepare(h, c, t))
		return PHI_OPEN_ERR;
	nml_http_client_conf(h->cl, c);
	h->state = ST_PROCESSING;

	if (!t->input.seek)
		t->input.seek = ~0ULL;

	return h;
}

static void httpcl_close(struct httpcl *h, phi_track *t)
{
	nml_http_client_free(h->cl);
	ffvec_free(&h->path);
	ffstr_free(&h->conf.headers);
	phi_track_free(t, h);
}

static int httpcl_process(struct httpcl *h, phi_track *t)
{
	if (t->input.seek != ~0ULL) {
		warnlog(t, "seeking isn't supported");
		t->input.seek = ~0ULL;
	}

	switch (h->state) {
	case ST_DATA:
		if (h->icy) {
			h->icy = 0;
			if (!core->track->filter(h->trk, &phi_icy, 0))
				return PHI_ERR;
		}

		t->data_out = h->data;
		h->state = ST_PROCESSING;
		return (h->done) ? PHI_DONE : PHI_DATA;

	case ST_PROCESSING:
		h->state = ST_WAIT;
		nml_http_client_run(h->cl);
		return PHI_ASYNC;

	case ST_ERR:
	default:
		return PHI_ERR;
	}
}

const phi_filter phi_http = {
	httpcl_open, (void*)httpcl_close, (void*)httpcl_process,
	"http-client"
};


static const void* net_iface(const char *name)
{
	if (ffsz_eq(name, "client")) return &phi_http;
	return NULL;
}

static void net_close()
{
#ifndef PHI_HTTP_NO_SSL
	nml_ssl_uninit(phi_ssl_ctx);
	if (phi_ssl_ctx) {
		ffmem_free(phi_ssl_ctx->ctx_conf->cert_file);
		ffmem_free(phi_ssl_ctx->ctx_conf);
		ffmem_free(phi_ssl_ctx);
	}
#endif
}

static const phi_mod phi_net_mod = {
	.ver = PHI_VERSION, .ver_core = PHI_VERSION_CORE,
	net_iface, net_close
};

FF_EXPORT const phi_mod* phi_mod_init(const phi_core *_core)
{
	core = _core;
	ffsock_init(FFSOCK_INIT_SIGPIPE | FFSOCK_INIT_WSA | FFSOCK_INIT_WSAFUNCS);
	return &phi_net_mod;
}
