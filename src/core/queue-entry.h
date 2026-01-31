/** phiola: queue entry
2023, Simon Zolin */

#include <ffsys/path.h>
#include <ffsys/dirscan.h>

static int qe_heal_ext(char *fn);


struct q_entry_bukt {
	uint offsets[8];
};

static struct q_entry_bukt* qeb_alloc(uint cap)
{
	cap += ffint_align_ceil2(sizeof(struct q_entry_bukt), 64);
	char *p = ffmem_align(cap, 64);
	ffmem_zero(p, 64);
	return (void*)p;
}

static void qeb_free(struct q_entry_bukt *eb) { ffmem_alignfree(eb); }

static void* qeb_data(struct q_entry_bukt *eb) { return (char*)eb + 64; }

/** Mark the region at `ptr` as used */
static uint qeb_add(struct q_entry_bukt *eb, uint i, void *ptr)
{
	return (eb->offsets[i] = (char*)ptr - (char*)eb);
}

/** Mark the region as free.
Return 0 if the bucket becomes free. */
static uint qeb_unref(struct q_entry_bukt *eb, uint off)
{
	uint sum = 0;
	for (uint i = 0;  i < 8;  i++) {
		if (off == eb->offsets[i])
			eb->offsets[i] = 0;
		sum |= eb->offsets[i];
	}
	return sum;
}


struct q_entry {
	struct phi_queue_entry pub;
	struct phi_queue *q;
	phi_track *trk;
	uint index;
	uint used;
	uint bucket_offset;
	uint expand :1;
	uint play_next_on_close :1;
	char name[0];
};

static void qe_free(struct q_entry *e)
{
	if (e->q && e == e->q->cursor)
		e->q->cursor = NULL;
	if (e == qm->cursor)
		qm->cursor = NULL;

	core->metaif->destroy(&e->pub.meta);
	if (e->pub.url != e->name)
		ffmem_free(e->pub.url);

	if (e->bucket_offset) {
		struct q_entry_bukt *eb = (void*)((char*)e - e->bucket_offset);
		if (!qeb_unref(eb, e->bucket_offset))
			qeb_free(eb);
		return;
	}

	ffmem_alignfree(e);
}

static void qe_init(struct q_entry *e, uint url_len, const struct phi_queue_entry *qe)
{
	ffmem_zero_obj(e);
	e->pub = *qe;
	e->used = 1;
	ffmem_copy(e->name, qe->url, url_len);
	e->pub.url = e->name;
}

static struct q_entry* qe_new(struct phi_queue_entry *qe)
{
	size_t n = ffsz_len(qe->url) + 1;
	struct q_entry *e = ffmem_align(sizeof(struct q_entry) + n, 64);
	qe_init(e, n, qe);
	return e;
}

static void qe_new_bulk(const struct phi_queue_entry *qe, uint n, struct q_entry* result[])
{
	FF_ASSERT(n <= 8);
	uint url_len[8];
	uint cap = 0;
	for (uint i = 0;  i < n;  i++) {
		url_len[i] = ffsz_len(qe[i].url) + 1;
		cap += ffint_align_ceil2(sizeof(struct q_entry) + url_len[i], 64);
	}
	struct q_entry_bukt *eb = qeb_alloc(cap);
	struct q_entry *e = qeb_data(eb);
	for (uint i = 0;  i < n;  i++) {
		qe_init(e, url_len[i], qe + i);
		e->bucket_offset = qeb_add(eb, i, e);
		result[i] = e;
		e = (void*)((char*)e + ffint_align_ceil2(sizeof(struct q_entry) + url_len[i], 64));
	}
}

struct q_entry* qe_ref(struct q_entry *e)
{
	e->used++;
	return e;
}

int qe_unref(struct q_entry *e)
{
	FF_ASSERT(e->used);
	if (1 != ffint_fetch_add(&e->used, -1))
		return 1;

	qe_free(e);
	return 0;
}

static void* qe_open(phi_track *t) { return t->qent; }

static void qe_close(void *f, phi_track *t)
{
	struct q_entry *e = f;
	if (e->trk == t) {
		e->trk = NULL;

		if (e->q && !e->q->conf.conversion && !e->pub.meta_priority) {
			int mod = (META_LEN(&e->pub.meta) || META_LEN(&t->meta)); // empty meta == not modified
			if (META_LEN(&t->meta)) {
				fflock_lock((fflock*)&e->pub.lock); // UI thread may read or write `conf.meta` at this moment
				core->metaif->destroy(&e->pub.meta);
				e->pub.meta = t->meta; // Remember the tags we read from file in this track
				fflock_unlock((fflock*)&e->pub.lock);
				meta_zero(&t->meta);
			}

			if (t->audio.total != ~0ULL && t->audio.format.rate) {
				uint64 duration_msec = samples_to_msec(t->audio.total, t->audio.format.rate);
				e->pub.length_sec = duration_msec / 1000;
			}

			if (mod)
				q_modified(e->q);
		}

		if (e->expand || t->meta_reading)
			core->track->stop(t);
	}

	core->metaif->destroy(&t->meta);
	ffmem_free(t->conf.afilter.equalizer);
	t->conf.afilter.equalizer = NULL;

	if (e->q) {
		uint flags = (t->error) ? Q_TKCL_ERR : 0;
		if ((t->chain_flags & (PHI_FSTOP | PHI_FSTOP_AFTER)) // track stopped by user
			|| (e->expand && !e->play_next_on_close)) // The expanding process has finished without 'play' command issued on the root QE
			flags |= Q_TKCL_STOP;
		flags |= (t->meta_reading) ? Q_TKCL_META_READ : 0;
		q_ent_closed(e->q, flags);
	}
	qe_unref(e);
}

static int qe_process(void *f, phi_track *t)
{
	struct q_entry *e = f;
	if (t->meta_reading) {
		if (!fffile_exists(e->pub.url)) {
			if (qe_heal_ext(e->pub.url))
				return PHI_FIN;
		}
	}

	t->data_out = t->data_in;
	return PHI_DONE;
}

static const phi_filter phi_queue_guard = {
	qe_open, qe_close, qe_process,
	"queue-guard"
};

static int qe_play(struct q_entry *e)
{
	if (e->expand) {
		e->play_next_on_close = 1; // wait until expanding is finished and then play next track
		dbglog("play_next_on_close = 1");
		return 1;
	}

	struct phi_track_conf c = e->q->conf.tconf;
	c.ifile.name = e->pub.url;
	c.seek_cdframes = e->pub.seek_cdframes;
	c.until_cdframes = e->pub.until_cdframes;
	c.afilter.equalizer = ffsz_dup_safe(c.afilter.equalizer);
	const phi_filter *ui_if = (e->q->conf.ui_module_if_set) ? e->q->conf.ui_module_if : core->mod(e->q->conf.ui_module);

	const phi_track_if *track = core->track;
	phi_track *t = track->create(&c);

	if (e->q->conf.first_filter
		&& !track->filter(t, e->q->conf.first_filter, 0))
		goto err;

	if (e->q->conf.conversion) {
		if (!track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| (c.afilter.danorm
				&& !track->filter(t, core->mod("af-danorm.f"), 0))
			|| !track->filter(t, ui_if, 0)
			|| (c.afilter.gain_db
				&& !track->filter(t, core->mod("afilter.gain"), 0))
			|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
			|| !track->filter(t, core->mod("format.auto-write"), 0)
			|| !track->filter(t, core->mod("core.auto-output"), 0))
			goto err;
		t->output.allow_async = 1;

	} else if (e->q->conf.analyze) {
		if (!track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| !track->filter(t, ui_if, 0)
			|| ((c.afilter.peaks_info
				|| c.afilter.loudness_summary)
				&& !track->filter(t, core->mod("afilter.auto-conv-f"), 0))
			|| (c.afilter.peaks_info
				&& !track->filter(t, core->mod("afilter.peaks"), 0))
			|| (c.afilter.loudness_summary
				&& !track->filter(t, core->mod("af-loudness.analyze"), 0)))
			goto err;

	} else {
		if (!track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| (c.tee && !c.tee_output
				&& !track->filter(t, core->mod("core.tee"), 0))
			|| !track->filter(t, core->mod("format.detect"), 0)
			|| !track->filter(t, core->mod("afilter.until"), 0)
			|| !track->filter(t, ui_if, 0)
			|| (c.afilter.rg_normalizer
				&& !track->filter(t, core->mod("afilter.rg-norm"), 0))
			|| (c.afilter.auto_normalizer
				&& (!track->filter(t, core->mod("afilter.auto-conv-f"), 0)
				|| !track->filter(t, core->mod("af-loudness.analyze"), 0)
				|| !track->filter(t, core->mod("afilter.auto-norm"), 0)))
			|| (c.afilter.equalizer
				&& !track->filter(t, core->mod("af-sox.sox"), 0))
			|| !track->filter(t, core->mod("afilter.gain"), 0)
			|| !track->filter(t, core->mod("afilter.auto-conv"), 0)
			|| (c.tee_output
				&& !track->filter(t, core->mod("core.tee"), 0))
			|| !track->filter(t, core->mod(e->q->conf.audio_module), 0))
			goto err;
	}

	if (META_LEN(&e->q->conf.tconf.meta))
		core->metaif->copy(&t->meta, &e->q->conf.tconf.meta, 0); // from user
	if (META_LEN(&e->pub.meta) && e->pub.meta_priority)
		core->metaif->copy(&t->meta, &e->pub.meta, (META_LEN(&e->q->conf.tconf.meta)) ? PHI_META_UNIQUE : 0); // from .cue

	e->trk = t;
	e->used++;
	e->q->active_n++;

	t->qent = &e->pub;
	track->start(t);
	return 0;

err:
	ffmem_free(c.afilter.equalizer);
	track->close(t);
	return -1;
}

static int qe_expand(struct q_entry *e)
{
	const phi_track_if *track = core->track;
	if (url_checkz(e->pub.url))
		return -1;

	ffbool dir = 0, decompress = 0;
	fffileinfo fi;
	if (!fffile_info_path(e->pub.url, &fi)
		&& fffile_isdir(fffileinfo_attr(&fi))) {
		dir = 1;
	} else {
		ffstr ext;
		ffpath_splitname_str(FFSTR_Z(e->pub.url), NULL, &ext);
		if (!(ffstr_eqz(&ext, "m3u8")
			|| ffstr_eqz(&ext, "m3u")
			|| (decompress = ffstr_eqz(&ext, "m3uz"))))
			return -1;
	}

	struct phi_track_conf c = e->q->conf.tconf;
	c.ifile.name = e->pub.url;
	phi_track *t = track->create(&c);
	if (dir) {
		if (!track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.dir-read"), 0))
			goto err;
	} else {
		if (!track->filter(t, &phi_queue_guard, 0)
			|| !track->filter(t, core->mod("core.auto-input"), 0)
			|| (decompress
				&& !track->filter(t, core->mod("zstd.decompress"), 0))
			|| !track->filter(t, core->mod("format.m3u"), 0))
			goto err;
	}

	e->expand = 1;
	e->trk = t;
	e->used++;
	e->q->active_n++;

	t->qent = &e->pub;
	track->start(t);
	return 0;

err:
	track->close(t);
	return -1;
}

/** Find an existing file with the same name but different extension.
/path/dir/file.mp3 -> /path/dir/file.m4a */
static int qe_heal_ext(char *fn)
{
	int rc = 1;
	ffdirscan ds = {};
	ffstr ss, name, dir;
	ffpath_splitpath_str(FFSTR_Z(fn), &dir, &name);
	ffpath_splitname_str(name, &name, NULL);
	char *dirz = ffsz_dupstr(&dir);
	if (!dir.len || !name.len)
		goto end;

	char wc[4] = {
		name.ptr[0],
		'*',
		'\0'
	};
	ds.wildcard = wc;
	if (ffdirscan_open(&ds, dirz, FFDIRSCAN_NOSORT | FFDIRSCAN_USEWILDCARD))
		goto end;

	const char *s;
	for (;;) {
		if (!(s = ffdirscan_next(&ds)))
			goto end;
		ss = FFSTR_Z(s);
		if (ffstr_match2(&ss, &name) && s[name.len] == '.')
			break;
	}

	if (ss.len > ffsz_len(fn))
		goto end; // not supported
	dbglog("list heal: '%s' -> %s", fn, ss.ptr);
	ffmem_copy(name.ptr + name.len + 1, s + name.len + 1, ss.len - (name.len + 1) + 1); // replace extension (also write NUL)
	rc = 0;

end:
	if (rc)
		dbglog("list heal: %s: didn't find similar file in '%s'", fn, dirz);
	ffmem_free(dirz);
	ffdirscan_close(&ds);
	return rc;
}

static void qe_read_meta(struct q_entry *e)
{
	struct phi_track_conf c = e->q->conf.tconf;
	c.ifile.name = e->pub.url;
	c.info_only = 1;
	phi_track *t = core->track->create(&c);
	core->track->filter(t, &phi_queue_guard, 0);
	core->track->filter(t, core->mod("core.auto-input"), 0);
	core->track->filter(t, core->mod("format.detect"), 0);

	e->trk = t;
	e->used++;
	e->q->active_n++;

	t->meta_reading = 1;
	t->qent = &e->pub;
	core->track->start(t);
}

static void qe_stop(struct q_entry *e)
{
	if (e->trk)
		core->track->stop(e->trk);
}

static int qe_index(struct q_entry *e)
{
	if (!e->q) return -1;

	if (q_get(e->q, e->index) != e) {
		e->index = q_find(e->q, e);
	}
	return e->index;
}

static void* qe_insert(struct q_entry *e, struct phi_queue_entry *qe)
{
	return q_insert(e->q, qe_index(e) + 1, qe);
}

static void* qe_insert_bulk(struct q_entry *e, struct phi_queue_entry *qe, uint n, struct phi_queue_entry **result)
{
	if (!n) return &e->pub;
	return q_insert_bulk(e->q, qe_index(e) + 1, qe, n, result);
}

static int qe_remove(struct q_entry *e)
{
	return q_remove_at(e->q, qe_index(e), 1);
}

/** Check if the item matches the filter */
int qe_filter(struct q_entry *e, ffstr filter, uint flags)
{
	ffstr name, val;

	if (flags & PHI_QF_FILENAME) {
		name = FFSTR_Z(e->pub.url);
		if (ffstr_ifindstr(&name, &filter) >= 0)
			return 1;
	}

	if (flags & PHI_QF_META) {
		uint i = 0;
		while (core->metaif->list(&e->pub.meta, &i, &name, &val, 0)) {
			if (ffstr_ifindstr(&val, &filter) >= 0)
				return 1;
		}
	}

	return 0;
}

static phi_queue_id qe_queue(struct q_entry *e)
{
	return e->q;
}

int qe_rename(struct q_entry *e, const char *new, uint flags)
{
	int rc = -1;
	if (fffile_exists(new)) {
		errlog("file already exists: \"%s\"", new);
		goto end;
	}
	if (fffile_rename(e->pub.url, new)) {
		syserrlog("fffile_rename: \"%s\"", new);
		goto end;
	}
	infolog("file renamed: \"%s\" -> \"%s\"", e->pub.url, new);

	if (e->pub.url == e->name
		&& ffsz_len(new) <= ffsz_len(e->name)) {
		ffsz_copyz(e->name, -1, new);
	} else {
		if (e->pub.url != e->name)
			ffmem_free(e->pub.url);
		e->pub.url = (char*)new,  new = NULL;
	}

	rc = 0;

end:
	if (flags & PHI_QRN_ACQUIRE)
		ffmem_free((char*)new);
	return rc;
}

/** All printable, plus SPACE, except: ", *, /, :, <, >, ?, \, | */
static const uint _ffpath_charmask_filename[] = {
	0,
	            // ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!
	0x2bff7bfb, // 0010 1011 1111 1111  0111 1011 1111 1011
	            // _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@
	0xefffffff, // 1110 1111 1111 1111  1111 1111 1111 1111
	            //  ~}| {zyx wvut srqp  onml kjih gfed cba`
	0x6fffffff, // 0110 1111 1111 1111  1111 1111 1111 1111
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff
};

static void q_rename_first_close(void *f, phi_track *t)
{
	struct q_entry *e = t->qent;
	ffstr s, val, dir, ext;
	ffvec buf = {};

	// Extract file meta components to prepare the target file name
	ffstr out = FFSTR_INITZ(e->q->rename_pattern);
	while (out.len) {
		if ('v' == ffstr_var_next(&out, &s, '@')) {
			ffstr_shift(&s, 1);
			core->metaif->find(&t->meta, s, &val, 0);
			ffvec_grow(&buf, val.len, 1);
			buf.len += ffpath_makename(ffslice_end(&buf, 1), -1, val, '_', _ffpath_charmask_filename);

		} else {
			ffvec_addstr(&buf, &s);
		}
	}

	ffpath_split3_str(FFSTR_Z(t->conf.ifile.name), &dir, NULL, &ext);
	if (dir.len)
		dir.len++; // "dir" -> "dir/"
	if (ext.len)
		ext.ptr--,  ext.len++; // "ext" -> ".ext"
	char *fn = ffsz_allocfmt("%S%S%S", &dir, &buf, &ext);
	qe_rename(t->qent, fn, PHI_QRN_ACQUIRE);

	qe_unref(e);
	core->metaif->destroy(&t->meta);
	ffvec_free(&buf);
	q_rename_next(e->q);
}

static int q_rename_first_process(void *f, phi_track *t)
{
	return PHI_DONE;
}

static const phi_filter q_rename_first = {
	NULL, q_rename_first_close, q_rename_first_process,
	"q-rename-first"
};

void qe_rename_start(struct q_entry *e)
{
	struct phi_track_conf c = {
		.ifile.name = e->pub.url,
		.info_only = 1,
	};
	phi_track *t = core->track->create(&c);

	core->track->filter(t, e->q->conf.first_filter, 0);
	core->track->filter(t, &q_rename_first, 0);
	core->track->filter(t, core->mod("core.auto-input"), 0);
	core->track->filter(t, core->mod("format.detect"), 0);

	e->used++;
	t->qent = &e->pub;
	core->track->start(t);
}
