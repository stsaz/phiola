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
};

static void* icy_open(phi_track *t)
{
	struct icy *c = ffmem_new(struct icy);
	c->metaif = core->mod("format.meta");
	icyread_open(&c->icy, t->icy_meta_interval);
	return c;
}

static void icy_close(void *ctx, phi_track *t)
{
	struct icy *c = ctx;
	ffmem_free(c);
}

static int icy_meta(struct icy *c, ffstr data, phi_track *t)
{
	dbglog(t, "meta: [%L] \"%S\"", data.len, &data);

	ffstr artist = {}, title = {};
	for (;;) {
		ffstr k, v;
		if (0 != icymeta_read(&data, &k, &v))
			break;

		if (ffstr_ieqz(&k, "StreamTitle"))
			icymeta_artist_title(v, &artist, &title);
	}

	ffvec utf = {};

	if (!ffutf8_valid_str(artist)) {
		ffstr_growadd_codepage((ffstr*)&utf, &utf.cap, artist.ptr, artist.len, FFUNICODE_WIN1252);
		artist = *(ffstr*)&utf;
		utf.len = 0;
	}
	c->metaif->destroy(&t->meta);
	c->metaif->set(&t->meta, FFSTR_Z("artist"), artist, 0);

	if (!ffutf8_valid_str(title)) {
		ffstr_growadd_codepage((ffstr*)&utf, &utf.cap, title.ptr, title.len, FFUNICODE_WIN1252);
		title = *(ffstr*)&utf;
	}
	c->metaif->set(&t->meta, FFSTR_Z("title"), title, 0);

	t->meta_changed = 1;
	ffvec_free(&utf);
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
