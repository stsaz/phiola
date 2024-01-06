/** libmpg123 interface
2016, Simon Zolin */

#include "mpg123-phi.h"
#include <mpg123.h>
#include <mpg123lib_intern.h>
#include <frame.h>

#define ERR(r)  ((r > 0) ? -r : r - 1000)

const char* phi_mpg123_error(int e)
{
	e = (e > -1000) ? -e : e + 1000;
	return mpg123_plain_strerror(e);
}


int phi_mpg123_init()
{
	return mpg123_init();
}

struct phi_mpg123 {
	mpg123_handle *h;
	unsigned new_fmt :1;
};

int phi_mpg123_open(phi_mpg123 **pm, unsigned int flags)
{
	int r;
	phi_mpg123 *m;

	if (NULL == (m = calloc(1, sizeof(phi_mpg123))))
		return ERR(MPG123_OUT_OF_MEM);

	m->h = mpg123_new(NULL, &r);
	if (r != MPG123_OK) {
		free(m);
		return ERR(r);
	}

	mpg123_param(m->h, MPG123_ADD_FLAGS, MPG123_QUIET | MPG123_IGNORE_INFOFRAME | flags, .0);

	if (MPG123_OK != (r = mpg123_open_feed(m->h))) {
		phi_mpg123_free(m);
		return ERR(r);
	}

	m->new_fmt = 1;
	*pm = m;
	return 0;
}

void phi_mpg123_free(phi_mpg123 *m)
{
	mpg123_delete(m->h);
	mpg123_exit();
	free(m);
}

void phi_mpg123_reset(phi_mpg123 *m)
{
	m->h->rdat.buffer.fileoff = 1;
	m->h->rdat.filepos = 0;
	INT123_feed_set_pos(m->h, 0);
	INT123_frame_buffers_reset(m->h);
}

int phi_mpg123_decode(phi_mpg123 *m, const char *data, size_t size, unsigned char **audio)
{
	int r;

	if (size != 0
		&& MPG123_OK != (r = mpg123_feed(m->h, (void*)data, size)))
		return ERR(r);

	if (audio == NULL)
		return 0;

	size_t bytes;
	r = mpg123_decode_frame(m->h, NULL, audio, &bytes);
	if (m->new_fmt && r == MPG123_NEW_FORMAT) {
		m->new_fmt = 0;
		r = mpg123_decode_frame(m->h, NULL, audio, &bytes);
	}
	if (r == MPG123_NEED_MORE)
		return 0;
	else if (r != MPG123_OK)
		return ERR(r);
	return bytes;
}

int INT123_compat_open(const char *filename, int flags){return 0;}
