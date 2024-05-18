/** phiola: read ICY data
2021, Simon Zolin */

#include <track.h>
#include <avpack/icy.h>

extern const phi_core *core;
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct icy {
	icyread icy;
	const phi_meta_if *metaif;
	ffstr data;
	ffvec meta;
};

static void* icy_open(phi_track *t)
{
	struct icy *c = phi_track_allocT(t, struct icy);
	c->metaif = core->mod("format.meta");
	icyread_open(&c->icy, t->icy_meta_interval);
	return c;
}

static void icy_close(void *ctx, phi_track *t)
{
	struct icy *c = ctx;
	ffvec_free(&c->meta);
	phi_track_free(t, c);
}

static int icy_meta(struct icy *c, ffstr data, phi_track *t)
{
	dbglog(t, "meta: [%L] \"%S\"", data.len, &data);
	if (c->meta.len == data.len
		&& !ffmem_cmp(c->meta.ptr, data.ptr, data.len)) {
		return 0; // meta hasn't really changed
	}
	c->meta.len = 0;
	ffvec_add2(&c->meta, &data, 1);

	ffstr artist = {}, title = {};
	for (;;) {
		ffstr k, v;
		if (0 != icymeta_read(&data, &k, &v))
			break;

		if (ffstr_ieqz(&k, "StreamTitle"))
			icymeta_artist_title(v, &artist, &title);
	}

	ffvec utf = {}, utf2 = {};

	if (!ffutf8_valid_str(artist)) {
		ffstr_growadd_codepage((ffstr*)&utf, &utf.cap, artist.ptr, artist.len, FFUNICODE_WIN1252);
		artist = *(ffstr*)&utf;
	}
	if (!ffutf8_valid_str(title)) {
		ffstr_growadd_codepage((ffstr*)&utf2, &utf2.cap, title.ptr, title.len, FFUNICODE_WIN1252);
		title = *(ffstr*)&utf2;
	}

	ffstr martist = {}, mtitle = {};
	c->metaif->find(&t->meta, FFSTR_Z("title"), &mtitle, 0);
	c->metaif->find(&t->meta, FFSTR_Z("artist"), &martist, 0);
	if (!ffstr_ieq2(&title, &mtitle)
		|| !ffstr_ieq2(&artist, &martist)) {
		c->metaif->destroy(&t->meta);
		c->metaif->set(&t->meta, FFSTR_Z("artist"), artist, 0);
		c->metaif->set(&t->meta, FFSTR_Z("title"), title, 0);
		t->meta_changed = 1;
	}

	ffvec_free(&utf);
	ffvec_free(&utf2);
	return 0;
}

static int icy_process(void *ctx, phi_track *t)
{
	struct icy *c = ctx;

	if (t->chain_flags & PHI_FSTOP) {
		return PHI_LASTOUT;
	}

	if (t->chain_flags & PHI_FFWD) {
		c->data = t->data_in;
		t->data_in.len = 0;
	}

	for (;;) {

		ffstr s;
		int r = icyread_process(&c->icy, &c->data, &s);
		switch (r) {
		case ICYREAD_MORE:
			return PHI_MORE;

		case ICYREAD_DATA:
			t->data_out = s;
			return PHI_DATA;

		case ICYREAD_META:
			icy_meta(c, s, t);
			break;
		}
	}
}

const phi_filter phi_icy = {
	icy_open, icy_close, icy_process,
	"icy"
};
