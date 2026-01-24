/** libsox wrapper
2026, Simon Zolin */

#include "sox-phi.h"
#include <stdlib.h>
#include <string.h>
#include <sox.h>

sox_effects_chain_t * sox_create_effects_chain3(sox_encodinginfo_t const * in_enc, sox_encodinginfo_t const * out_enc, void *opaque);
sox_effect_t * sox_create_effect2(sox_effect_handler_t const * eh, void *opaque);

struct sox_ctx {
	struct sox_conf conf;

	sox_effects_chain_t *chain;
	sox_signalinfo_t signal;

	size_t input_len4;
	const sox_sample_t *input;

	size_t output_len4;
	const sox_sample_t *output;
};

static void* mem_alloc(void *opaque, unsigned n) { return malloc(n); }
static void mem_free(void *opaque, void *ptr) { free(ptr); }

void* lsx_ualloc(void *opaque, size_t n)
{
	sox_ctx *c = opaque;
	void *ptr;
	if (!(ptr = c->conf.mem_alloc(c->conf.opaque, n)))
		abort();
	return ptr;
}

void lsx_ufree(void *opaque, void *ptr)
{
	sox_ctx *c = opaque;
	c->conf.mem_free(c->conf.opaque, ptr);
}

void phi_sox_destroy(sox_ctx *c)
{
	if (!c) return;

	if (c->chain)
		sox_delete_effects_chain(c->chain);
	lsx_ufree(c, c);
}


static int input_drain(sox_effect_t *e, sox_sample_t *obuf, size_t *osamp)
{
	struct sox_ctx *c = e->opaque;
	if (*osamp > c->input_len4)
		*osamp = c->input_len4;
	memcpy(obuf, c->input, *osamp * 4);
	c->input_len4 -= *osamp;
	c->input += *osamp;
	return SOX_EOF;
}

static const sox_effect_handler_t input_handler = {
	"input", NULL, SOX_EFF_MCHAN, NULL, NULL, NULL, input_drain, NULL, NULL,
};


static int output_flow(sox_effect_t *e, const sox_sample_t *ibuf, sox_sample_t *obuf, size_t *isamp, size_t *osamp)
{
	struct sox_ctx *c = e->opaque;
	c->output = ibuf;
	c->output_len4 = *isamp;
	*osamp = 0;
	return (c->output_len4) ? SOX_EOF : 0;
}

static const sox_effect_handler_t output_handler = {
	"output", NULL, SOX_EFF_MCHAN, NULL, NULL, output_flow, NULL, NULL, NULL,
};


int phi_sox_create(sox_ctx **pc, struct sox_conf *conf)
{
	sox_ctx *c = (conf->mem_alloc) ? conf->mem_alloc(conf->opaque, sizeof(sox_ctx)) : malloc(sizeof(sox_ctx));
	memset(c, 0, sizeof(*c));
	c->conf = *conf;

	if (!c->conf.mem_alloc) {
		c->conf.mem_alloc = mem_alloc;
		c->conf.mem_free = mem_free;
	}

	sox_signalinfo_t signal = {
		.rate = conf->rate,
		.channels = conf->channels,
	};
	c->signal = signal;

	sox_encodinginfo_t encoding = {};
	c->chain = sox_create_effects_chain3(&encoding, &encoding, c);

	sox_effect_t *e = sox_create_effect2(&input_handler, c);
	sox_add_effect(c->chain, e, &signal, &signal);
	free(e);

	*pc = c;
	return 0;

err:
	phi_sox_destroy(c);
	return 1;
}

/*
sox_effects_chain_t.effects[] -> sox_effect_t[channels] -> sox_effect_t.priv
*/
int phi_sox_filter(sox_ctx *c, const char *name, const char* argv[], unsigned argc)
{
	int rc = 1;
	sox_effect_t *e = NULL;

	if (!name) {
		e = sox_create_effect2(&output_handler, c);
		goto add;
	}

	const sox_effect_handler_t *eh;
	if (!(eh = sox_find_effect(name)))
		goto end;
	e = sox_create_effect2(eh, c);

	if (argc
		&& sox_effect_options(e, argc, (char**)argv))
		goto end;

add:
	if (sox_add_effect(c->chain, e, &c->signal, &c->signal))
		goto end;
	rc = 0;

end:
	if (rc)
		lsx_ufree(c, e->priv);
	free(e);
	return rc;
}

int phi_sox_process(sox_ctx *c, const int *input, size_t *len, int **output)
{
	c->input = input;
	c->input_len4 = *len / 4;
	c->output_len4 = 0;
	sox_flow_effects(c->chain, NULL, NULL);
	*len -= c->input_len4 * 4;
	*output = (int*)c->output;
	return c->output_len4 * 4;
}


static void output_message(unsigned level, const char *filename, const char *fmt, va_list ap) {}

static sox_globals_t s_sox_globals = {
	2,
	output_message,
	sox_false,
	8192,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	sox_false,
	sox_false,
	10
};

sox_globals_t* sox_get_globals() { return &s_sox_globals; }

static sox_effects_globals_t s_sox_effects_globals = {sox_plot_off, &s_sox_globals};

sox_effects_globals_t* sox_get_effects_globals()
{
	return &s_sox_effects_globals;
}

#define SOX_MESSAGE_FUNCTION(name,level) \
void name(char const * fmt, ...) { \
	va_list ap; \
	va_start(ap, fmt); \
	if (sox_globals.output_message_handler) \
		(*sox_globals.output_message_handler)(level,sox_globals.subsystem,fmt,ap); \
	va_end(ap); \
}

SOX_MESSAGE_FUNCTION(lsx_fail_impl  , 1)
SOX_MESSAGE_FUNCTION(lsx_warn_impl  , 2)
SOX_MESSAGE_FUNCTION(lsx_report_impl, 3)
SOX_MESSAGE_FUNCTION(lsx_debug_impl , 4)
SOX_MESSAGE_FUNCTION(lsx_debug_more_impl , 5)
SOX_MESSAGE_FUNCTION(lsx_debug_most_impl , 6)

void init_fft_cache() {}
void clear_fft_cache() {}
