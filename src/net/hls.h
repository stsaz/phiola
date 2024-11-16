/** phiola: HLS reader
2024, Simon Zolin */

/*
HLS client algorithm:
. Repeat:
	. Request .m3u8 by HTTP and receive its data
	. Parse the data and get file names
	. Determine which files are new from the last time (using #EXT-X-MEDIA-SEQUENCE value)
		. If there are no new files, wait
	. Add new files to the queue
	. Repeat:
		. Pop the first data file from the queue
			. If the queue is empty, exit the loop
		. Prepare a complete URL for the file (base URL for .m3u8 file + file name)
		. Request the data file by HTTP
		. Pass file data to the next filters
*/

/*
Call map:
	... [phiola]
	httpcl_process()
		nml_http_client_run()
			... [.m3u] [netmill]
				phi_hc_data()
				on_complete()
	hls_f_request()
		nml_http_client_run()
			... [.ogg] [netmill]
				phi_hc_data()
	... [phiola]
	httpcl_process()
			... [.ogg] [netmill]
				on_complete()
	hls_f_request()
		...
*/

struct hlsread {
	ffvec		data, url;
	ffvec		q; // ffstr[]
	ffstr		base_url;
	uint64		seq_last;
	phi_timer	wait;
	phi_task	task;
	uint		worker;
	uint		n_fail_tries;
	uint		data_file :1;
};

static struct hlsread* hls_new(struct httpcl *h)
{
	struct hlsread *l = ffmem_new(struct hlsread);
	ffpath_splitpath_str(FFSTR_Z(h->trk->conf.ifile.name), &l->base_url, NULL);
	l->worker = h->trk->worker;
	return l;
}

static void hls_free(struct httpcl *h, struct hlsread *l)
{
	if (!l) return;

	ffvec_free(&l->data);
	ffstr *it;
	FFSLICE_WALK(&l->q, it) {
		ffstr_free(it);
	}
	ffvec_free(&l->q);
	ffvec_free(&l->url);
	core->timer(l->worker, &l->wait, 0, NULL, NULL);
	ffmem_free(l);
}

/** Received new data chunk */
static int hls_data(struct httpcl *h, ffstr data)
{
	struct hlsread *l = h->hls;
	if (!l->data_file) {
		ffvec_addstr(&l->data, &data);
		return 1;
	}
	return 0;
}

/** Retrieve new data files from m3u list.
Return 0 if something was added */
static int hls_q_update(struct httpcl *h, ffstr d)
{
	struct hlsread *l = h->hls;
	dbglog(h->trk, "m3u content: %S", &d);

	int n = 0;
	ffstr ln, s;
	uint64 seq = 0;
	while (d.len) {
		ffstr_splitby(&d, '\n', &ln, &d);
		ffstr_rskipchar1(&ln, '\r');

		if (ffstr_matchz(&ln, "#")) {
			if (ffstr_matchz(&ln, "#EXT-X-MEDIA-SEQUENCE:")) {
				s = ln;
				ffstr_shift(&s, FFS_LEN("#EXT-X-MEDIA-SEQUENCE:"));
				if (!ffstr_to_uint64(&s, &seq))
					warnlog(h->trk, "incorrect '#EXT-X-MEDIA-SEQUENCE' value: %S", &ln);
			}
			continue;
		}

		if (l->seq_last < seq) {
			l->seq_last = seq;
			ffstr *ps = ffvec_zpushT(&l->q, ffstr);
			ffstr_dupstr(ps, &ln);
			n++;
		}
		seq++;
	}
	return !n;
}

/** Get the first data file from the queue.
name: User must free the value with ffstr_free() */
static int hls_q_get(struct httpcl *h, ffstr *name)
{
	struct hlsread *l = h->hls;
	if (!l->q.len)
		return -1;

	*name = *ffslice_itemT(&l->q, 0, ffstr);
	ffslice_rmT((ffslice*)&l->q, 0, 1, ffstr);
	dbglog(h->trk, "playing data file %S [%L]"
		, name, l->q.len);
	return 0;
}

/** Request file from server */
static void hls_f_request(struct httpcl *h, ffstr name)
{
	struct hlsread *l = h->hls;
	nml_http_client_free(h->cl);
	ffstr_free(&h->conf.headers);
	ffmem_zero_obj(&h->conf);

	h->cl = nml_http_client_create();

	struct nml_http_client_conf *c = &h->conf;
	ffstr url = FFSTR_Z(h->trk->conf.ifile.name);
	l->data_file = 0;
	if (name.len) {
		l->url.len = 0;
		ffvec_addfmt(&l->url, "%S/%S", &l->base_url, &name);
		ffstr_free(&name);
		url = *(ffstr*)&l->url;
		l->data_file = 1;
	}

	conf_prepare(h, c, h->trk, url);
	dbglog(h->trk, "requesting %S", &url);
	nml_http_client_conf(h->cl, c);
	nml_http_client_run(h->cl);
}

static void hls_f_next(void *param)
{
	struct httpcl *h = param;
	ffstr name = {};
	hls_q_get(h, &name);

	if (ffstr_matchz(&name, "http://")
		|| ffstr_matchz(&name, "https://")) {
		errlog(h->trk, "expecting relative file name: '%S'", &name);
		if (FF_SWAP(&h->state, ST_ERR) == ST_WAIT)
			core->track->wake(h->trk);
		return;
	}

	hls_f_request(h, name);
}

/** HTTP request is complete */
static int hls_f_complete(struct httpcl *h)
{
	struct hlsread *l = h->hls;
	if (!l->data_file) {
		int r = hls_q_update(h, *(ffstr*)&l->data);
		l->data.len = 0;
		if (r) {
			if (!l->seq_last
				|| l->n_fail_tries == 10) {
				errlog(h->trk, "no more files in m3u");
				return -1;
			}

			l->data_file = 0;
			dbglog(h->trk, "no new files in m3u, waiting");
			l->n_fail_tries++;
			core->timer(l->worker, &l->wait, -2000, hls_f_next, h);
			return 0;
		}

		l->n_fail_tries = 0;
	}

	core->task(l->worker, &l->task, hls_f_next, h);
	return 0;
}
