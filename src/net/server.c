/** phiola: ICY server
2025, Simon Zolin */

/*
                                                          -> consumer -> ac.enc -> format.write -> server ->
                                                        (ring)                                          (ring)
file.read -> format.read -> ac.dec -> af.conv -> provider ->                                              -> http.server.conn
...
*/

#include <track.h>
#include <util/util.h>
#include <http-server/conn.h>
#include <ffbase/ring.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "audio-server", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "audio-server", t, __VA_ARGS__)
#define userlog(t, ...)  phi_userlog(core, "audio-server", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "audio-server", t, __VA_ARGS__)

extern const struct nml_http_server_if nml_http_server_interface;
extern const struct nml_tcp_listener_if nml_tcp_listener_interface;

#define AUSV_CLIENT_BUF_SIZE_KB  16

struct ausv {
	nml_http_server *sv;
	ffvec clients_paused; // nml_http_sv_conn*[]
	struct nml_address addr;
	phi_track *trk, *subtrack;
	phi_task task;
	phi_timer tmr;
	const phi_queue_if *qif;
	ffring *iring, *oring;
	ffring_head rh;
	ffvec meta;
	ffstr input;
	uint64 total_msec;
	uint64 next_pos_samples;
	size_t buf_half_samples;
	size_t half_pos;
	uint worker;
	uint clients;
	uint qi;
	uint pkt;
	uint ogg_opus :1;
	uint consumer_paused :1;
	uint provider_paused :1;
	uint output_full :1;
};
static struct ausv *gs;

static void ausv_track_closed(struct ausv *s, uint stop);

static void ausv_provider_paused(struct ausv *s)
{
	s->provider_paused = 1;
	if (s->consumer_paused) {
		s->consumer_paused = 0;
		core->track->wake(s->trk);
	}
}

static void ausv_consumer_paused(struct ausv *s)
{
	s->consumer_paused = 1;
	if (s->provider_paused) {
		s->provider_paused = 0;
		core->track->wake(s->subtrack);
	}
}

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
	s->clients--;

	// Remove this client from the list of suspended clients
	nml_http_sv_conn **hsc;
	FFSLICE_WALK(&s->clients_paused, hsc) {
		if (*hsc == c) {
			size_t i = hsc - (nml_http_sv_conn**)s->clients_paused.ptr;
			ffslice_rmswapT((ffslice*)&s->clients_paused, i, 1, void*);
			break;
		}
	}
}

static void ausv_client_unpause(struct ausv *s)
{
	if (s->clients_paused.len) {
		// Notify all suspended clients to continue their work
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


static void nml_log(void *log_obj, uint level, const char *ctx, const char *id, const char *format, ...)
{
	struct ausv *s = log_obj;
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
	core->conf.logv(core->conf.log_obj, level, "audio-server", s->trk, format, va);
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
};

static int phi_sv_open(nml_http_sv_conn *c)
{
	struct ausv *s = c->conf->opaque;
	struct ausv_cl *sc = ffmem_new(struct ausv_cl);
	sc->rpos = s->oring->rtail;
	c->proxy = sc;

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

	ausv_client_connected(s, c);
	hs_response(c, HTTP_200_OK);
	c->req_no_chunked = 1;
	ffvec_alloc(&c->file.buf, AUSV_CLIENT_BUF_SIZE_KB * 1024, 1);
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

static inline int ring_peek_at(ffring *r, size_t off, size_t n, ffstr *d1, ffstr *d2)
{
	n = ffmin(n, r->wtail - off);
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

	ffstr d1, d2;
	size_t n = ring_peek_at(s->oring, sc->rpos, ffvec_unused(buf), &d1, &d2);
	if (!n) {
		ausv_client_paused(s, c);
		return NMLR_ASYNC;
	}
	ffmem_copy(buf->ptr + buf->len, d1.ptr, d1.len);
	ffmem_copy(buf->ptr + buf->len + d1.len, d2.ptr, d2.len);
	buf->len += n;
	sc->rpos += n;

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
};


static void* ausv_grd_open(phi_track *t)
{
	return (void*)1;
}

static void ausv_grd_close(void *f, phi_track *t)
{
	core->track->stop(t);
	struct ausv *s = t->udata;
	ausv_track_closed(s, !!(t->chain_flags & PHI_FSTOP));
}

static int ausv_grd_process(void *f, phi_track *t)
{
	return PHI_DONE;
}

/** Receives notification when an input track is finished */
static const phi_filter ausv_guard = {
	ausv_grd_open, ausv_grd_close, ausv_grd_process,
	"ausv-guard"
};


static void* ausv_provider_open(phi_track *t)
{
	struct ausv *s = t->udata;
	userlog(s->trk, "New track: %s", t->conf.ifile.name);
	return t->udata;
}

static int ausv_provider_process(struct ausv *s, phi_track *t)
{
	if (t->chain_flags & PHI_FSTOP)
		return PHI_FIN;

	if (!s->input.len && (t->chain_flags & PHI_FFWD)) {
		s->input = t->data_in;
		t->data_in.len = 0;
	}

	size_t n = ffring_writestr(s->iring, s->input);
	if (n < s->input.len) {
		ffstr_shift(&s->input, n);
		ausv_provider_paused(s);
		return PHI_ASYNC;
	}
	ffstr_shift(&s->input, n);
	dbglog(s->trk, "ibuffer: %L%%", (s->iring->wtail - s->iring->rtail) * 100 / s->iring->cap);
	return !(t->chain_flags & PHI_FFIRST) ? PHI_MORE : PHI_DONE;
}

/** Sends data from input track to the main track */
static const phi_filter ausv_provider = {
	ausv_provider_open, NULL, (void*)ausv_provider_process,
	"ausv-provider"
};


static void* ausv_consumer_open(phi_track *t)
{
	return t->udata;
}

static int ausv_consumer_process(struct ausv *s, phi_track *t)
{
	if (t->chain_flags & PHI_FSTOP)
		return PHI_FIN;

	if (!(t->chain_flags & PHI_FFWD)) {
		ffring_read_finish(s->iring, s->rh);
	}

	ffstr d;
	s->rh = ffring_read_begin(s->iring, ~0U, &d, NULL);
	if (!d.len) {
		ausv_consumer_paused(s);
		return PHI_ASYNC;
	}

	s->total_msec += bytes_to_msec_af(d.len, &t->oaudio.format);
	t->data_out = d;
	return PHI_DATA;
}

/** Receives data from input track */
const struct phi_filter ausv_consumer = {
	ausv_consumer_open, NULL, (void*)ausv_consumer_process,
	"ausv-consumer",
};


static void ausv_close_delayed(struct ausv *s)
{
	FF_ASSERT(s->clients == 0);
	FF_ASSERT(s->subtrack == NULL);
	nml_http_server_interface.free(s->sv);
	ffring_free(s->iring);
	ffring_free(s->oring);
	ffvec_free(&s->meta);
	ffvec_free(&s->clients_paused);
	phi_track_free(s->trk, s);
	gs = NULL;
}

static void ausv_close(struct ausv *s, phi_track *t)
{
	core->timer(t->worker, &s->tmr, 0, NULL, NULL);
	core->task(t->worker, &s->task, (void*)ausv_close_delayed, s);
}

static phi_track* ausv_track_start(struct ausv *s)
{
	uint i = s->qi++;
	if (i >= (uint)s->qif->count(NULL)) {
		if (i == 0)
			return NULL; // empty playlist
		s->qi = 0;
		i = 0;
	}
	const struct phi_queue_entry *qe = s->qif->at(NULL, i);
	struct phi_track_conf tc = {
		.ifile.name = qe->url,
		.oaudio.format = s->trk->conf.oaudio.format,
	};

	const phi_track_if *track = core->track;
	phi_track *t = track->create(&tc);
	if (!track->filter(t, &ausv_guard, 0)
		|| !track->filter(t, core->mod("core.auto-input"), 0)
		|| !track->filter(t, core->mod("format.detect"), 0)
		|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
		|| !track->filter(t, &ausv_provider, 0)) {
		track->close(t);
		return NULL;
	}

	t->udata = s;
	track->start(t);
	return t;
}

static void ausv_track_closed(struct ausv *s, uint stop)
{
	if (stop) {
		s->subtrack = NULL;
		return;
	}
	s->subtrack = ausv_track_start(s);
}

static void ausv_timer(void *param)
{
	struct ausv *s = param;

	ffstr d1, d2;
	ffring_head rh = ffring_read_all_begin(s->oring, s->half_pos - s->oring->rtail, &d1, &d2, NULL);
	ffring_read_finish(s->oring, rh);

	s->next_pos_samples += s->buf_half_samples;

	if (s->output_full) {
		s->output_full = 0;
		core->track->wake(s->trk);
	}

	userlog(s->trk, "[%U:%02U:%02U.%03U]  Listeners:%u"
		, s->total_msec / (60*60*1000)
		, (s->total_msec / 60000) % 60
		, (s->total_msec / 1000) % 60
		, s->total_msec % 1000
		, s->clients);
}

static void* ausv_open(phi_track *t)
{
	const struct phi_asv_conf *ac = *(struct phi_asv_conf**)&t->conf.ofile.mtime;

	core->track->filter(t, &ausv_consumer, PHI_TF_PREV);
	core->track->filter(t, core->mod("format.auto-write"), PHI_TF_PREV);

	t->oaudio.format = t->conf.oaudio.format;
	t->data_type = "pcm";

	struct ausv *s = phi_track_allocT(t, struct ausv);
	gs = s;
	s->qif = core->mod("core.queue");
	t->udata = s;
	s->ogg_opus = ffsz_eq(t->conf.ofile.name, "stream.opus");
	s->trk = t;
	s->worker = t->worker;

	uint n = msec_to_bytes_af(t->conf.oaudio.buf_time, &t->oaudio.format);
	s->iring = ffring_alloc(n, FFRING_1_READER | FFRING_1_WRITER);
	uint kbps = (s->ogg_opus) ? t->conf.opus.bitrate : t->conf.aac.quality;
	n = msec_to_bytes_kbps(t->conf.oaudio.buf_time, kbps);
	s->oring = ffring_alloc(n, FFRING_1_READER | FFRING_1_WRITER);

	s->next_pos_samples = s->buf_half_samples = msec_to_samples(t->conf.oaudio.buf_time / 2, t->oaudio.format.rate);
	core->timer(s->worker, &s->tmr, t->conf.oaudio.buf_time / 2, ausv_timer, s);

	s->sv = nml_http_server_interface.create();
	struct nml_http_server_conf sc;
	nml_http_server_interface.conf(NULL, &sc);
	sc.response.server_name = FFSTR_Z("phiola/2");
	sc.opaque = s;
	sc.boss = s;
	sc.server.wif = &nmlwrk_if;
	sc.server.lsif = &nml_tcp_listener_interface;
	s->addr.port = ac->port;
	sc.server.listen_addresses = &s->addr;
	if (ac->max_clients)
		sc.server.max_connections = ac->max_clients;
	sc.chain = sv_chain;

	sc.log_level = NML_LOG_VERBOSE;
	if (core->conf.log_level >= PHI_LOG_EXTRA)
		sc.log_level = NML_LOG_EXTRA;
	else if (core->conf.log_level >= PHI_LOG_DEBUG)
		sc.log_level = NML_LOG_DEBUG;
	sc.log = nml_log;
	sc.log_obj = s;

	if (nml_http_server_interface.conf(s->sv, &sc)) {
		ausv_close(s, t);
		return PHI_OPEN_ERR;
	}

	userlog(t, "Started ICY/HTTP server (TCP port %u)", s->addr.port);
	s->subtrack = ausv_track_start(s);
	nml_http_server_interface.run(s->sv);
	return s;
}

static int ausv_process(struct ausv *s, phi_track *t)
{
	if (t->chain_flags & PHI_FSTOP)
		return PHI_FIN;

	if (s->ogg_opus && s->pkt <= 1) {
		if (!t->data_in.len)
			return PHI_MORE;
		s->pkt++;
		ffvec_addstr(&s->meta, &t->data_in); // Store OGG Opus header and tags
		return PHI_MORE;
	}

	if (!t->data_in.len)
		return PHI_MORE;

	// Determine how many bytes to discard from the ring buffer on next timer signal
	if (t->audio.pos < s->next_pos_samples) {
		s->half_pos = s->oring->wtail;
		dbglog(s->trk, "apos:%U  next:%U  half:%L"
			, t->audio.pos, s->next_pos_samples, s->half_pos);
	}

	size_t n = ffring_write_all(s->oring, t->data_in.ptr, t->data_in.len);
	if (n == 0) {
		// There is no free space for this compressed audio frame -> suspend processing until the timer signals
		s->output_full = 1;
		return PHI_ASYNC;
	}
	dbglog(s->trk, "obuffer: %L%%", (s->oring->wtail - s->oring->rtail) * 100 / s->oring->cap);
	ausv_client_unpause(s);
	return PHI_MORE;
}

const struct phi_filter phi_audiosv = {
	ausv_open, (void*)ausv_close, (void*)ausv_process,
	"audio-server",
};
