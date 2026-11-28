/** phiola: HTTP remote control server
2026, Simon Zolin */

#include <track.h>
#include <netmill-http.h>
#include <ffbase/json-writer.h>
#include <ffbase/json-scheme.h>
#include <ffsys/random.h>

#ifndef PHI_HTTP_NO_SSL
#include <util/ssl.h>
#include <openssl/rand.h>
#endif

extern const phi_core *core;
#define errlog(...)  phi_errlog(core, "http-ctl", NULL, __VA_ARGS__)
#define warnlog(...)  phi_warnlog(core, "http-ctl", NULL, __VA_ARGS__)
#define userlog(...)  phi_userlog(core, "http-ctl", NULL, __VA_ARGS__)
#define dbglog(...)  phi_dbglog(core, "http-ctl", NULL, __VA_ARGS__)

extern const struct nml_http_server_if nml_http_server_interface;
extern const struct nml_tcp_listener_if nml_tcp_listener_interface;
extern uint phi_nml_log_level(uint phi_level);
extern uint nml_phi_log_level(uint nml_level);

struct hc_play;
static void hcsv_track_ev(struct hc_play *pl, uint flags);
static double volume_db;
static uint volume_pct;

#include <net/http-ctl-play.h>
#include <afilter/pcm.h>

struct http_ctl_sv {
	nml_tcp_listener *tl;
	struct nml_http_server_conf htsv_conf;
	struct nml_address addr;
	const phi_queue_if *queue;
	char *www_path;
	ffvec http_resp;
	uint worker;
	ffstr pass, set_cookie_hdr;
	char session_key[32];
	struct nml_ssl_ctx *ssl_ctx;

	char date_buf[FFS_LEN("0000-00-00T00:00:00.000")+1];
	uint conn_id;

	struct hc_play *pl;
};

static struct http_ctl_sv *hs;

static void hcsv_track_ev(struct hc_play *pl, uint flags)
{
	switch (flags) {
	case 0:
		hs->pl = pl;
		break;
	case 1:
		if (hs->pl == pl)
			hs->pl = NULL;
		break;
	}
}

#include <net/http-ctl-nml.h>


static void hcsv_resp(nml_http_sv_conn *c, int status)
{
	if (status != HTTP_200_OK)
		hs_response_err(c, status);
	else
		hs_response(c, status);
	c->resp.content_length = 0;
	c->resp_done = 1;
}

static int hcsv_auth(nml_http_sv_conn *c)
{
#ifdef PHI_REMOTE_NO_AUTH
	return 0;
#endif

	ffstr ck = HS_REQUEST_DATA(c, c->req.cookie), name, val;
	while (ck.len) {
		ffstr_splitby(&ck, ';', &val, &ck);
		ffstr_trimwhite(&val);
		ffstr_splitby(&val, '=', &name, &val);
		if (ffstr_eqz(&name, "phiola_sess"))
			return !ffstr_eq(&val, hs->session_key, sizeof(hs->session_key));
	}
	return -1;
}

static void hs_resp_401(nml_http_sv_conn *c)
{
	c->resp.code = 401;
	ffstr_setz(&c->resp.msg, "Unauthorized");
	c->resp.content_length = 0;
	c->resp_done = 1;
}

static void hs_resp_204(nml_http_sv_conn *c)
{
	c->resp.code = 204;
	ffstr_setz(&c->resp.msg, "No Content");
	c->resp.content_length = 0;
	c->resp_done = 1;
}


static void hcsv_play_stop(nml_http_sv_conn *c)
{
	if (hcsv_auth(c)) {
		hs_resp_401(c);
		return;
	}
	core->track->cmd(NULL, PHI_TRACK_STOP_ALL);
	hs_resp_204(c);
}

static void hcsv_play_play(nml_http_sv_conn *c)
{
	struct http_ctl_sv *s = c->conf->opaque;
	if (hcsv_auth(c)) {
		hs_resp_401(c);
		return;
	}
	if (!hs->pl)
		s->queue->play(NULL, s->queue->at(NULL, 0));
	else if (s->pl->paused)
		hc_play_pause(s->pl);
	hs_resp_204(c);
}

static void hcsv_play_pause(nml_http_sv_conn *c)
{
	struct http_ctl_sv *s = c->conf->opaque;
	if (hcsv_auth(c)) {
		hs_resp_401(c);
		return;
	}
	if (s->pl && !s->pl->paused)
		hc_play_pause(s->pl);
	hs_resp_204(c);
}

static void hcsv_play_next(nml_http_sv_conn *c)
{
	struct http_ctl_sv *s = c->conf->opaque;
	if (hcsv_auth(c)) {
		hs_resp_401(c);
		return;
	}
	s->queue->play(NULL, PHI_Q_PLAY_NEXT);
	hs_resp_204(c);
}

static void hcsv_play_prev(nml_http_sv_conn *c)
{
	struct http_ctl_sv *s = c->conf->opaque;
	if (hcsv_auth(c)) {
		hs_resp_401(c);
		return;
	}
	s->queue->play(NULL, PHI_Q_PLAY_PREVIOUS);
	hs_resp_204(c);
}

struct hcsv_seek_req {
	ffint64 pos_pct;
};

static const ffjson_arg hcsv_seek_args[] = {
	{ "pos_pct", FFJSON_TINT, FF_OFF(struct hcsv_seek_req, pos_pct) },
	{},
};

static void hcsv_play_seek(nml_http_sv_conn *c)
{
	struct http_ctl_sv *s = c->conf->opaque;
	if (hcsv_auth(c)) {
		hs_resp_401(c);
		return;
	}
	struct hcsv_seek_req req = {};
	ffstr errmsg = {};
	int r = ffjson_parse_object(hcsv_seek_args, &req, &c->input, 0, 0, &errmsg);
	if (r < 0) {
		warnlog("seek: bad body: %S", &errmsg);
		ffstr_free(&errmsg);
		hcsv_resp(c, HTTP_400_BAD_REQUEST);
		return;
	}
	if (req.pos_pct < 0 || req.pos_pct > 100) {
		warnlog("seek: pos_pct out of range: %lld", (long long)req.pos_pct);
		hcsv_resp(c, HTTP_400_BAD_REQUEST);
		return;
	}
	hc_play_seek(s->pl, (uint)req.pos_pct);
	hs_resp_204(c);
}

struct hcsv_volume_req {
	ffint64 vol_pct;
};

static const ffjson_arg hcsv_volume_args[] = {
	{ "vol_pct", FFJSON_TINT, FF_OFF(struct hcsv_volume_req, vol_pct) },
	{},
};

static void hcsv_play_volume(nml_http_sv_conn *c)
{
	struct http_ctl_sv *s = c->conf->opaque;
	if (hcsv_auth(c)) {
		hs_resp_401(c);
		return;
	}
	struct hcsv_volume_req req = {};
	ffstr errmsg = {};
	int r = ffjson_parse_object(hcsv_volume_args, &req, &c->input, 0, 0, &errmsg);
	if (r < 0) {
		warnlog("volume: bad body: %S", &errmsg);
		ffstr_free(&errmsg);
		hcsv_resp(c, HTTP_400_BAD_REQUEST);
		return;
	}
	if (req.vol_pct < 0 || req.vol_pct > 100) {
		warnlog("volume: vol_pct out of range: %lld", (long long)req.vol_pct);
		hcsv_resp(c, HTTP_400_BAD_REQUEST);
		return;
	}

	volume_pct = req.vol_pct;
	volume_db = vol2db((uint)req.vol_pct, -40);
	userlog("Volume: %.02FdB", volume_db);
	hc_play_volume(s->pl);
	hs_resp_204(c);
}

static void hcsv_play_status(nml_http_sv_conn *c)
{
	struct http_ctl_sv *s = c->conf->opaque;
	if (hcsv_auth(c)) {
		hs_resp_401(c);
		return;
	}
	s->http_resp.len = 0;
	ffjsonw jw;
	ffjsonw_init(&jw);
	jw.buf = s->http_resp;

	ffjsonw_addobj(&jw, 1);
	if (!s->pl) {
		ffjsonw_addpairz(&jw, "status", "stopped");
	} else {
		struct play_track_status pts = {};
		hc_play_status(s->pl, &pts);

		ffjsonw_addpairz(&jw, "status", (pts.paused) ? "paused" : "playing");
		ffjsonw_addpair(&jw, &FFSTR_Z("artist"), &pts.artist);
		ffjsonw_addpair(&jw, &FFSTR_Z("title"), &pts.title);
		ffjsonw_addkeyz(&jw, "pos");
		ffjsonw_addint(&jw, pts.pos_sec);
		ffjsonw_addkeyz(&jw, "total");
		ffjsonw_addint(&jw, pts.total_sec);
	}
	ffjsonw_addkeyz(&jw, "volume");
	ffjsonw_addint(&jw, volume_pct);
	ffjsonw_addobj(&jw, 0);

	s->http_resp = jw.buf;
	ffvec_null(&jw.buf);
	ffjsonw_close(&jw);

	hcsv_resp(c, HTTP_200_OK);
	ffstr_setz(&c->resp.content_type, "application/json");
	c->resp.content_length = s->http_resp.len;
	c->output = ffvec_str(&s->http_resp);
}

struct hcsv_login_req {
	ffstr pass;
};

static const ffjson_arg hcsv_login_args[] = {
	{ "pass", FFJSON_TSTR, FF_OFF(struct hcsv_login_req, pass) },
	{},
};

static void hcsv_login(nml_http_sv_conn *c)
{
	struct http_ctl_sv *s = c->conf->opaque;
	struct hcsv_login_req req = {};
	ffstr errmsg = {};
	int r = ffjson_parse_object(hcsv_login_args, &req, &c->input, 0, 0, &errmsg);
	if (r < 0) {
		warnlog("login: bad body: %S", &errmsg);
		ffstr_free(&errmsg);
		hcsv_resp(c, HTTP_400_BAD_REQUEST);
		return;
	}

	r = !ffstr_eq2(&req.pass, &s->pass);
	ffstr_free(&req.pass);
	if (r) {
		warnlog("login failed");
		hcsv_resp(c, HTTP_403_FORBIDDEN);
		return;
	}

	userlog("login success");
	hcsv_resp(c, HTTP_200_OK);
	c->resp.headers = s->set_cookie_hdr;
}

static const struct nml_http_virtdoc hcsv_routes[] = {
	{ "/login",			"POST",	hcsv_login },
	{ "/play/next",		"POST",	hcsv_play_next },
	{ "/play/pause",	"POST",	hcsv_play_pause },
	{ "/play/play",		"POST",	hcsv_play_play },
	{ "/play/prev",		"POST",	hcsv_play_prev },
	{ "/play/seek",		"POST",	hcsv_play_seek },
	{ "/play/status",	"GET",	hcsv_play_status },
	{ "/play/stop",		"POST",	hcsv_play_stop },
	{ "/play/volume",	"POST",	hcsv_play_volume },
	{}
};


extern void hs_start(ffsock csock, const ffsockaddr *peer, uint conn_id, struct nml_http_server_conf *conf);

static void hcsv_on_accept(void *udata, ffsock csock, ffsockaddr *peer)
{
	struct http_ctl_sv *s = udata;
	uint conn_id = ffint_fetch_add(&s->conn_id, 1);
	hs_start(csock, peer, conn_id, &s->htsv_conf);
}

static void hcsv_on_complete(void *udata, ffsock sk, struct zzkevent *kev)
{
	struct http_ctl_sv *s = udata;
	ffsock_close(sk);
	if (kev)
		core->kev_free(s->worker, (phi_kevent*)kev);
}

static int hcsv_tcp_lisn_create(struct http_ctl_sv *s)
{
	struct nml_tcp_listener_conf lc;
	nml_tcp_listener_interface.conf(NULL, &lc);

	lc.log_level = phi_nml_log_level(core->conf.log_level);
	if (!lc.log_level)
		lc.log_level = NML_LOG_VERBOSE;
	lc.log = nml_log;
	lc.log_obj = s;

	lc.core = nmlcore;
	lc.on_accept = hcsv_on_accept;
	lc.opaque = s;

	lc.addr = s->addr;

	if (!(s->tl = nml_tcp_listener_interface.create()))
		return -1;
	return nml_tcp_listener_interface.conf(s->tl, &lc);
}

static void hcsv_free(struct http_ctl_sv *s)
{
	nml_http_file_uninit(&s->htsv_conf);
	nml_http_virtspace_uninit(&s->htsv_conf);

#ifndef PHI_HTTP_NO_SSL
	if (s->ssl_ctx) {
		nml_ssl_uninit(s->ssl_ctx);
		ffmem_free(s->ssl_ctx->ctx_conf);
		ffmem_free(s->ssl_ctx);
	}
#endif

	ffstr_free(&s->pass);
	ffstr_free(&s->set_cookie_hdr);
	ffmem_free(s->www_path);
	ffmem_free(s);
}

static void hcsv_close()
{
	if (hs)
		hcsv_free(hs);
	hs = NULL;
}

static int hcsv_create_start(struct phi_remote_sv_conf *conf)
{
	struct http_ctl_sv *s = ffmem_new(struct http_ctl_sv);
	volume_pct = 100;
	s->queue = core->mod("core.queue");
	s->worker = core->worker_assign(1);

	struct nml_http_server_conf *sc = &s->htsv_conf;
	nml_http_server_interface.conf(NULL, sc);
	sc->core = nmlcore;
	sc->boss = s;

	// Log
	sc->log_level = phi_nml_log_level(core->conf.log_level);
	if (!sc->log_level)
		sc->log_level = NML_LOG_VERBOSE;
	sc->log = nml_log;
	sc->log_obj = s;

	// HTTP
	sc->on_complete = hcsv_on_complete;
	sc->response.server_name = FFSTR_Z("phiola/2");
	s->www_path = ffsz_allocfmt("%S/mod/www", &core->conf.root);
	ffstr_setz(&sc->fs.www, s->www_path);
	nml_http_virtspace_init(sc, hcsv_routes);
	nml_http_file_init(sc, FFSTR_Z(
		"text/html html\r\n"
		"text/css css\r\n"
		"application/javascript js\r\n"
		"image/svg+xml svg\r\n"
		));
	sc->opaque = s;
	sc->chain = hcsv_chain;

#ifndef PHI_HTTP_NO_SSL
	if (conf->ssl_cert.len) {
		if (!(s->ssl_ctx = hcsv_ssl_prepare(s, conf->ssl_cert))) {
			errlog("can't initialize SSL context");
			goto err;
		}
		sc->ssl_ctx = s->ssl_ctx;
		sc->chain = hcsv_ssl_chain;
	}
#endif

	if (conf->pass.len) {
		ffstr_dup2(&s->pass, &conf->pass);

		u_char buf[16];
#ifndef PHI_HTTP_NO_SSL
		if (1 != RAND_bytes(buf, sizeof(buf))) {
			errlog("RAND_bytes");
			goto err;
		}
#else
		for (uint i = 0;  i < sizeof(buf)/4;  i++) {
			uint r = core->rand();
			ffmem_copy(buf + i*4, &r, 4);
		}
#endif
		ffs_format(s->session_key, sizeof(s->session_key), "%16xb", buf);

		ffsize cap = 0;
		ffstr_growfmt(&s->set_cookie_hdr, &cap, "Set-Cookie: phiola_sess=%32s; Path=/; HttpOnly; SameSite=Strict%s\r\n"
			, s->session_key, (conf->ssl_cert.len) ? "; Secure" : "");
	}

	// TCP
	s->addr.port = conf->port;
	if (hcsv_tcp_lisn_create(s))
		goto err;
	if (nml_tcp_listener_interface.run(s->tl))
		goto err;

	userlog("Started HTTP remote control server (%s port %u)"
		, (conf->ssl_cert.len) ? "TLS" : "TCP", s->addr.port);
	hs = s;
	return 0;

err:
	hcsv_free(s);
	return -1;
}

const phi_remote_sv_if hctl_sv_if = {
	hcsv_create_start, hcsv_close,
};
