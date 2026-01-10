/** libsox wrapper
2026, Simon Zolin */

#include "sox-phi.h"
#include <stdlib.h>
#include <string.h>
#include <sox.h>

struct sox_ctx {
	sox_effects_chain_t *chain;
	sox_signalinfo_t signal;

	size_t input_len4;
	const sox_sample_t *input;

	size_t output_len4;
	const sox_sample_t *output;
};

void phi_sox_destroy(sox_ctx *c)
{
	if (!c) return;

	if (c->chain)
		sox_delete_effects_chain(c->chain);
	free(c);
}

static int input_drain(sox_effect_t *e, sox_sample_t *obuf, size_t *osamp)
{
	struct sox_ctx *c = *(void**)e->priv;
	if (*osamp > c->input_len4)
		*osamp = c->input_len4;
	memcpy(obuf, c->input, *osamp * 4);
	c->input_len4 -= *osamp;
	c->input += *osamp;
	return SOX_EOF;
}

static const sox_effect_handler_t input_handler = {
	"input", NULL, SOX_EFF_MCHAN, NULL, NULL, NULL, input_drain, NULL, NULL,
	.priv_size = sizeof(void*)
};

static int output_flow(sox_effect_t *e, const sox_sample_t *ibuf, sox_sample_t *obuf, size_t *isamp, size_t *osamp)
{
	struct sox_ctx *c = *(void**)e->priv;
	c->output = ibuf;
	c->output_len4 = *isamp;
	*osamp = 0;
	return (c->output_len4) ? SOX_EOF : 0;
}

static const sox_effect_handler_t output_handler = {
	"output", NULL, SOX_EFF_MCHAN, NULL, NULL, output_flow, NULL, NULL, NULL,
	.priv_size = sizeof(void*)
};

int phi_sox_create(sox_ctx **pc, struct sox_conf *conf)
{
	sox_ctx *c = calloc(1, sizeof(struct sox_ctx));

	sox_signalinfo_t signal = {
		.rate = conf->rate,
		.channels = conf->channels,
	};
	c->signal = signal;

	sox_encodinginfo_t encoding = {};
	c->chain = sox_create_effects_chain(&encoding, &encoding);

	sox_effect_t *e = sox_create_effect(&input_handler);
	*(void**)e->priv = c;
	if (sox_add_effect(c->chain, e, &signal, &signal))
		goto err;
	free(e);  e = NULL;

	*pc = c;
	return 0;

err:
	free(e);
	phi_sox_destroy(c);
	return 1;
}

int phi_sox_filter(sox_ctx *c, const char *name, const char* argv[], unsigned argc)
{
	sox_effect_t *e = NULL;

	if (!name) {
		e = sox_create_effect(&output_handler);
		*(void**)e->priv = c;
		goto done;
	}

	const sox_effect_handler_t *eh;
	if (!(eh = sox_find_effect(name)))
		goto err;
	e = sox_create_effect(eh);

	if (argc
		&& sox_effect_options(e, argc, (char**)argv))
		goto err;

done:
	if (sox_add_effect(c->chain, e, &c->signal, &c->signal))
		goto err;
	free(e);  e = NULL;
	return 0;

err:
	free(e);
	return 1;
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
	2,               /* unsigned     verbosity */
	output_message,  /* sox_output_message_handler */
	sox_false,       /* sox_bool     repeatable */
	8192,            /* size_t       bufsiz */
	0,               /* size_t       input_bufsiz */
	0,               /* int32_t      ranqd1 */
	NULL,            /* char const * stdin_in_use_by */
	NULL,            /* char const * stdout_in_use_by */
	NULL,            /* char const * subsystem */
	NULL,            /* char       * tmp_path */
	sox_false,       /* sox_bool     use_magic */
	sox_false,       /* sox_bool     use_threads */
	10               /* size_t       log2_dft_min_size */
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
