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
#include <avpack/icy.h>
#include <ffbase/ring.h>

#ifndef PHI_HTTP_NO_SSL
#include <util/ssl.h>
#endif

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, "audio-server", t, __VA_ARGS__)
#define warnlog(t, ...)  phi_warnlog(core, "audio-server", t, __VA_ARGS__)
#define userlog(t, ...)  phi_userlog(core, "audio-server", t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, "audio-server", t, __VA_ARGS__)

extern const struct nml_http_server_if nml_http_server_interface;
extern const struct nml_tcp_listener_if nml_tcp_listener_interface;
extern uint phi_nml_log_level(uint phi_level);
extern uint nml_phi_log_level(uint nml_level);

#ifndef PHI_HTTP_NO_SSL
struct nml_ssl_ctx *sv_ssl_ctx;
#endif

#define AUSV_CLIENT_BUF_SIZE_KB  16
#define ERRORS_MAX  20

struct ausv {
	nml_http_server *sv;
	ffvec conns, clients_paused; // nml_http_sv_conn*[]
	struct nml_address addr;
	phi_track *trk, *subtrack;
	phi_task task, task_unpause;
	phi_timer tmr;
	const phi_queue_if *qif;
	ffring *iring, *oring;
	ffring_head rh;
	ffvec meta;
	fflock meta_lock;
	ffstr resp_headers;
	ffstr input;
	uint64 total_msec;
	uint64 next_pos_samples;
	size_t buf_half_samples;
	size_t half_pos;
	uint worker;
	uint clients;
	uint qi;
	uint pkt;
	uint meta_uid;
	u_char consecutive_errors;
	uint ogg_opus :1;
	uint consumer_paused :1;
	uint provider_paused :1;
	uint output_full :1;
};
static struct ausv *gs;

uint ausv_icy_meta_read(struct ausv *s, uint last_meta_uid, ffvec *buf);
#include <net/server-nml.h>

void ausv_mod_close()
{
#ifndef PHI_HTTP_NO_SSL
	nml_ssl_uninit(sv_ssl_ctx);
	if (sv_ssl_ctx) {
		ffmem_free(sv_ssl_ctx->ctx_conf);
		ffmem_free(sv_ssl_ctx);
	}
#endif
}

static void ausv_track_closed(struct ausv *s, uint stop, uint error);

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

static void* ausv_grd_open(phi_track *t)
{
	return (void*)1;
}

static void ausv_grd_close(void *f, phi_track *t)
{
	core->track->stop(t);
	struct ausv *s = t->udata;
	ausv_track_closed(s, !!(t->chain_flags & PHI_FSTOP), t->error);
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

static void ausv_icy_meta(struct ausv *s, ffstr artist, ffstr title)
{
	char buf[250];
	ffsz_format(buf, sizeof(buf), "%S - %S", &artist, &title);
	fflock_lock(&s->meta_lock);
	s->meta.len = 0;
	icymeta_add(&s->meta, FFSTR_Z("StreamTitle"), FFSTR_Z(buf));
	icymeta_fin(&s->meta);
	fflock_unlock(&s->meta_lock);
	s->meta_uid++;
}

/** Read latest metadata.
Thread: worker
Return metadata revision number. */
uint ausv_icy_meta_read(struct ausv *s, uint last_meta_uid, ffvec *buf)
{
	uint mu = FFINT_READONCE(s->meta_uid);
	if (last_meta_uid != mu) {
		fflock_lock(&s->meta_lock);
		buf->len = ffmem_ncopy(buf->ptr, buf->cap, s->meta.ptr, s->meta.len);
		fflock_unlock(&s->meta_lock);
		// Note: 'meta_uid' may be already updated; the caller will catch up next time
	}
	return mu;
}

static void* ausv_provider_open(phi_track *t)
{
	struct ausv *s = t->udata;

	ffstr artist = {}, title = {};
	core->metaif->find(&t->meta, FFSTR_Z("artist"), &artist, 0);
	core->metaif->find(&t->meta, FFSTR_Z("title"), &title, 0);

	if (!s->ogg_opus)
		ausv_icy_meta(s, artist, title);

	userlog(s->trk, "New track: \"%S - %S\" \"%s\""
		, &artist, &title, t->conf.ifile.name);
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
	core->sig(PHI_CORE_STOP);

	// Force-close all connections
	ffvec v = s->conns;
	ffvec_null(&s->conns); // for faster ausv_client_closed()
	nml_http_sv_conn **hsc;
	FFSLICE_WALK(&v, hsc) {
		(*hsc)->conf->cl_destroy(*hsc);
	}
	ffvec_free(&v);
	FF_ASSERT(s->clients == 0);

	nml_http_server_interface.free(s->sv);
	ffstr_free(&s->resp_headers);
	ffring_free(s->iring);
	ffring_free(s->oring);
	ffvec_free(&s->meta);
	ffvec_free(&s->clients_paused);
	phi_track_free(s->trk, s);
	gs = NULL;
}

static void ausv_close(struct ausv *s, phi_track *t)
{
	if (s->subtrack) {
		core->track->stop(s->subtrack);
		s->subtrack = NULL;
	}

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
		|| (s->trk->conf.afilter.rg_normalizer
			&& !track->filter(t, core->mod("afilter.rg-norm"), 0))
		|| !track->filter(t, core->mod("afilter.gain"), 0)
		|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
		|| !track->filter(t, &ausv_provider, 0)) {
		track->close(t);
		return NULL;
	}

	t->meta_autofree = 1;
	t->udata = s;
	track->start(t);
	return t;
}

static void ausv_track_closed(struct ausv *s, uint stop, uint error)
{
	if (stop) {
		s->subtrack = NULL;
		return;
	}

	if (error) {
		if (++s->consecutive_errors >= ERRORS_MAX) {
			errlog(s->trk, "Stopping after %u consecutive errors", ERRORS_MAX);
			return;
		}
	} else {
		s->consecutive_errors = 0;
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
	t->data_type = PHI_AC_PCM;

	struct ausv *s = phi_track_allocT(t, struct ausv);
	gs = s;
	s->qif = core->mod("core.queue");
	t->udata = s;
	s->ogg_opus = ffsz_eq(t->conf.ofile.name, "stream.opus");
	s->trk = t;
	s->worker = core->worker_assign(1);
	ffvec_allocT(&s->conns, (ac->max_clients) ? ac->max_clients : 10000, void*);

	uint n = msec_to_bytes_af(t->conf.oaudio.buf_time, &t->oaudio.format);
	s->iring = ffring_alloc(n, FFRING_1_READER | FFRING_1_WRITER);
	uint kbps = (s->ogg_opus) ? t->conf.opus.bitrate : t->conf.aac.quality;
	n = msec_to_bytes_kbps(t->conf.oaudio.buf_time, kbps);
	s->oring = ffring_alloc(n, FFRING_1_READER | FFRING_1_WRITER);

	if (!s->ogg_opus) {
		size_t cap = 0;
		ffstr_growfmt(&s->resp_headers, &cap, "icy-br:%u\r\nicy-metaint:%u\r\n"
			, kbps, AUSV_CLIENT_BUF_SIZE_KB * 1024);
		ffvec_alloc(&s->meta, 255 * 16, 1); // Max ICY meta size
	}

	s->next_pos_samples = s->buf_half_samples = msec_to_samples(t->conf.oaudio.buf_time / 2, t->oaudio.format.rate);
	core->timer(t->worker, &s->tmr, t->conf.oaudio.buf_time / 2, ausv_timer, s);

	if (ausv_create(s, ac, t)) {
		ausv_close(s, t);
		return PHI_OPEN_ERR;
	}

	userlog(t, "Started ICY/HTTP server (%s port %u)"
		, (ac->ssl_cert.len) ? "TLS" : "TCP", s->addr.port);
	s->subtrack = ausv_track_start(s);
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
	core->task(s->worker, &s->task_unpause, (void*)ausv_client_unpause, s);
	return PHI_MORE;
}

const struct phi_filter phi_audiosv = {
	ausv_open, (void*)ausv_close, (void*)ausv_process,
	"audio-server",
};
