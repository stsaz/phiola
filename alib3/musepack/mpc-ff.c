/** libmpc interface
2017, Simon Zolin */

#include "mpc-ff.h"
#include <mpc/mpcdec.h>
#include <libmpcdec/internal.h>

enum {
	MPC_EOK,
	MPC_EMEM,
	MPC_EHDR,
	MPC_EINCOMPLETE,
	MPC_EUNPROCESSED,
};

static const char* const errs[] = {
	"",
	"memory allocation",
	"bad header",
	"incomplete block",
	"unprocessed data inside block",
};

const char* mpc_errstr(int e)
{
	e = -e;
	if (e > sizeof(errs) / sizeof(*errs))
		return "";
	return errs[e];
}


struct mpc_ctx {
	mpc_decoder *d;
	mpc_streaminfo si;
	unsigned int block_frames;
	mpc_bits_reader br;
	const char *bufptr;
	size_t buflen;
	char buffer[MAX_FRAME_SIZE];
	unsigned int is_key_frame :1;
};

extern mpc_status streaminfo_read_header_sv8(mpc_streaminfo* si, const mpc_bits_reader * r_in, mpc_size_t block_size);

int mpc_decode_open(mpc_ctx **pc, const void *sh_block, size_t len)
{
	mpc_ctx *c;

	if (NULL == (c = calloc(1, sizeof(mpc_ctx))))
		return -MPC_EMEM;

	c->br.buff = (void*)sh_block;
	c->br.count = 8;
	if (0 != streaminfo_read_header_sv8(&c->si, &c->br, len)) {
		free(c);
		return -MPC_EHDR;
	}

	if (NULL == (c->d = mpc_decoder_init(&c->si))) {
		free(c);
		return -MPC_EMEM;
	}
	*pc = c;
	c->br.buff = NULL;
	c->br.count = 0;
	return 0;
}

void mpc_decode_free(mpc_ctx *c)
{
	mpc_decoder_exit(c->d);
	free(c);
}

void mpc_decode_input(mpc_ctx *c, const void *block, size_t len)
{
	c->br.count = 8;
	c->bufptr = block;
	c->buflen = len;
	c->block_frames = 1 << c->si.block_pwr;
	c->is_key_frame = 1;
}

int mpc_decode(mpc_ctx *c, float *pcm)
{
	if (c->block_frames == 0) {
		if (c->buflen > 1) {
			c->buflen = 0;
			return -MPC_EUNPROCESSED;
		}
		return 0;
	}

	/* Note: decoder may overread up to MAX_FRAME_SIZE bytes. */
	if (c->buflen < MAX_FRAME_SIZE) {
		memcpy(c->buffer, c->bufptr, c->buflen);
		memset(c->buffer + c->buflen, 0, sizeof(c->buffer) - c->buflen);
		c->br.buff = c->buffer;
	} else
		c->br.buff = (void*)c->bufptr;

	const char *buf = c->br.buff;
	mpc_frame_info fr;
	fr.buffer = pcm;
	fr.is_key_frame = c->is_key_frame;
	c->is_key_frame = 0;
	mpc_decoder_decode_frame(c->d, &c->br, &fr);
	c->bufptr += (char*)c->br.buff - buf;
	c->buflen -= (char*)c->br.buff - buf;
	if ((ssize_t)c->buflen < 0)
		return -MPC_EINCOMPLETE;
	c->block_frames--;
	return fr.samples;
}


struct mpc_seekctx {
	mpc_demux demux;
};

extern mpc_status mpc_demux_ST(mpc_demux * d);

int mpc_seekinit(mpc_seekctx **pc, const void *sh_block, size_t sh_len, const void *st_block, size_t st_len)
{
	mpc_seekctx *c;
	mpc_demux *d;

	if (sh_len > DEMUX_BUFFER_SIZE
		|| st_len > DEMUX_BUFFER_SIZE)
		return -MPC_EHDR;

	if (NULL == (c = calloc(1, sizeof(mpc_seekctx))))
		return -MPC_EMEM;
	d = &c->demux;

	memcpy(d->buffer, sh_block, sh_len);
	d->bits_reader.buff = d->buffer;
	d->bits_reader.count = 8;
	if (0 != streaminfo_read_header_sv8(&d->si, &d->bits_reader, sh_len)) {
		mpc_seekfree(c);
		return -MPC_EHDR;
	}

	memcpy(d->buffer, st_block, st_len);
	d->bits_reader.buff = d->buffer;
	d->bits_reader.count = 8;
	mpc_demux_ST(d);

	*pc = c;
	return 0;
}

void mpc_seekfree(mpc_seekctx *c)
{
	free(c->demux.seek_table);
	free(c);
}

long long mpc_seek(mpc_seekctx *c, unsigned int *blk)
{
	mpc_demux *d = &c->demux;
	uint64_t off;
	unsigned int n = *blk;

	n >>= d->seek_pwr - d->si.block_pwr;
	if (n >= d->seek_table_size)
		n = d->seek_table_size - 1;
	off = d->seek_table[n] / 8;
	n <<= d->seek_pwr - d->si.block_pwr;

	*blk = n;
	return off;
}
