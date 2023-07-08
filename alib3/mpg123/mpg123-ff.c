/** libmpg123 interface
2016, Simon Zolin */

#include "mpg123-ff.h"
#include <mpg123.h>
#include <mpg123lib_intern.h>
#include <frame.h>

#define ERR(r)  ((r > 0) ? -r : r - 1000)

const char* mpg123_errstr(int e)
{
	e = (e > -1000) ? -e : e + 1000;
	return mpg123_plain_strerror(e);
}


struct mpg123 {
	mpg123_handle *h;
	unsigned int new_fmt :1;
};

int mpg123_open(mpg123 **pm, unsigned int flags)
{
	int r;
	mpg123 *m;

	if (NULL == (m = calloc(1, sizeof(mpg123))))
		return ERR(MPG123_OUT_OF_MEM);

	m->h = mpg123_new(NULL, &r);
	if (r != MPG123_OK) {
		free(m);
		return ERR(r);
	}

	mpg123_param(m->h, MPG123_ADD_FLAGS, MPG123_QUIET | MPG123_IGNORE_INFOFRAME | flags, .0);

	if (MPG123_OK != (r = mpg123_open_feed(m->h))) {
		mpg123_free(m);
		return ERR(r);
	}

	m->new_fmt = 1;
	*pm = m;
	return 0;
}

void mpg123_free(mpg123 *m)
{
	mpg123_delete(m->h);
	mpg123_exit();
	free(m);
}

void mpg123_reset(mpg123 *m)
{
	m->h->rdat.buffer.fileoff = 1;
	m->h->rdat.filepos = 0;
	feed_set_pos(m->h, 0);
	frame_buffers_reset(m->h);
}

int mpg123_decode(mpg123 *m, const char *data, size_t size, unsigned char **audio)
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

int parse_new_id3(mpg123_handle *fr, unsigned long first4bytes){}
void do_equalizer(real *bandPtr,int channel, real equalizer[2][32]){}
void do_equalizer_3dnow(real *bandPtr,int channel, real equalizer[2][32]){}
int compat_open(const char *filename, int flags){return 0;}
int compat_close(int infd){return 0;}
void mpg123_free_string(mpg123_string* sb){}
