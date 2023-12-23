/** phiola: pass input data to a file
2023, Simon Zolin */

#include <ffbase/ring.h>

static const phi_meta_if *meta_if;

struct tee_brg {
	uint		users;
	ffring*		ring;
	ffring_head	rhead;
};

static struct tee_brg* tee_brg_new(size_t buf_size)
{
	struct tee_brg *tb = ffmem_new(struct tee_brg);
	tb->users = 1;
	tb->ring = ffring_alloc(buf_size, FFRING_1_READER | FFRING_1_WRITER);
	return tb;
}

static void tee_brg_unref(struct tee_brg *tb)
{
	if (!tb) return;

	if (1 == ffint_fetch_add(&tb->users, -1)) {
		ffring_free(tb->ring);
		ffmem_free(tb);
	}
}

static void* tee_brg_open(phi_track *t)
{
	return t->udata;
}

static void tee_brg_close(void *f, phi_track *t)
{
	struct tee_brg *tb = f;
	tee_brg_unref(tb);
	meta_if->destroy(&t->meta);
	ffmem_free(t->conf.ofile.name);
}

static inline ffsize ffring_used(ffring *b)
{
	return FFINT_READONCE(b->wtail) - FFINT_READONCE(b->rhead);
}

static int tee_brg_process(void *f, phi_track *t)
{
	struct tee_brg *tb = f;

	if (!(t->chain_flags & PHI_FFWD)) {
		ffring_read_finish(tb->ring, tb->rhead);
		dbglog(t, "ring -%L [%L]", tb->rhead.nu - tb->rhead.old, ffring_used(tb->ring));
	}

	tb->rhead = ffring_read_begin(tb->ring, tb->ring->cap, &t->data_out, NULL);
	if (!t->data_out.len) {
		if (t->chain_flags & PHI_FSTOP)
			return PHI_DONE;
		return PHI_ASYNC;
	}
	return PHI_DATA;
}

static const phi_filter phi_tee_brg = {
	tee_brg_open, tee_brg_close, tee_brg_process,
	"tee-brg"
};


struct tee {
	uint			state;
	phi_track*		out_trk;
	struct tee_brg*	brg;
	uint			o_stdout;
};

static void* tee_open(phi_track *t)
{
	const char *tee_name = (t->conf.tee) ? t->conf.tee : t->conf.tee_output;
	ffstr fn, ext;
	ffpath_splitname_str(FFSTR_Z(tee_name), &fn, &ext);
	uint o_stdout = ffstr_eqz(&fn, "@stdout");

	if (t->conf.tee_output
		&& !ffstr_eqz(&ext, "wav")) {
		errlog(t, "-dup: output extension must be .wav");
		return PHI_OPEN_ERR;
	}

	if (t->conf.tee_output
		&& !t->oaudio.format.interleaved) {
		errlog(t, "-dup: non-interleaved audio is not supported");
		return PHI_OPEN_ERR;
	}

	if (t->conf.tee && o_stdout) {
		// icy -> tee -> stdout -> ... -> audio.output
		if (!core->track->filter(t, core->mod("core.stdout"), 0))
			return PHI_OPEN_ERR;
		t->data_out = t->data_in;
		return PHI_OPEN_SKIP;
	}

	struct tee *c = ffmem_new(struct tee);
	c->o_stdout = o_stdout;
	if (!meta_if)
		meta_if = core->mod("format.meta");
	return c;
}

static void tee_close(void *f, phi_track *t)
{
	struct tee *c = f;
	if (c->out_trk)
		core->track->stop(c->out_trk);
	tee_brg_unref(c->brg);
	ffmem_free(c);
}

static int tee_process(void *f, phi_track *t)
{
	struct tee *c = f;
	uint start_track = 0, wake_track = 0;
	enum { I_INIT, I_WAIT_META, I_DATA };

	switch (c->state) {
	case I_INIT:
		c->state = I_WAIT_META; // begin waiting for meta data
		break;

	case I_DATA:
		if (!(t->chain_flags & PHI_FFWD))
			return PHI_MORE;

		if (!t->meta_changed) {
			wake_track = 1;
			break; // pass data through
		}

		core->track->stop(c->out_trk); // close current file
		c->out_trk = NULL;
		tee_brg_unref(c->brg);
		c->brg = NULL;

		c->state = I_WAIT_META; // begin waiting for meta data
		// fallthrough

	case I_WAIT_META: {
		if (!t->meta.len) {
			if (!(t->chain_flags & PHI_FFWD))
				return PHI_MORE;

			break; // continue waiting for meta data
		}

		// new meta data

		/*
		icy -> tee -> ... -> audio.output
		          \
		           -> tee-brg -> file.write
		*/

		const char *tee_name = (t->conf.tee) ? t->conf.tee : t->conf.tee_output;
		struct phi_track_conf conf = {
			.ofile.name = ffsz_dup(tee_name),
		};
		c->out_trk = core->track->create(&conf);
		meta_if->copy(&c->out_trk->meta, &t->meta);

		if (t->conf.tee_output) {
			c->out_trk->data_type = "pcm";
			c->out_trk->oaudio.format = t->oaudio.format;
		}

		const char *writer = (c->o_stdout) ? "core.stdout" : "core.file-write";
		if (!core->track->filter(c->out_trk, &phi_tee_brg, 0)
			|| (t->conf.tee_output
				&& !core->track->filter(c->out_trk, core->mod("format.wav-write"), 0))
			|| !core->track->filter(c->out_trk, core->mod(writer), 0))
			return PHI_ERR;

		start_track = 1;
		c->state = I_DATA;
		break;
	}
	}

	if (!c->brg)
		c->brg = tee_brg_new(10*3000*1024/8); // 10 seconds of audio at 3000kbit/sec

	if (t->data_in.len != ffring_writestr(c->brg->ring, t->data_in))
		warnlog(t, "ring buffer is full");
	else
		dbglog(t, "ring +%L [%L]", t->data_in.len, ffring_used(c->brg->ring));

	if (start_track) {
		c->brg->users = 2;
		c->out_trk->udata = c->brg;
		core->track->start(c->out_trk);
	} else if (wake_track) {
		core->track->wake(c->out_trk);
	}

	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_DATA;
}

const phi_filter phi_tee = {
	tee_open, tee_close, tee_process,
	"tee"
};
