/** phiola: real-time peaks filter
2015,2022, Simon Zolin */

#include <track.h>
#include <afilter/pcm_maxpeak.h>

extern const phi_core *core;
#define errlog(t, ...)  phi_errlog(core, NULL, t, __VA_ARGS__)
#define dbglog(t, ...)  phi_dbglog(core, NULL, t, __VA_ARGS__)

struct rtpeak {
	struct phi_af fmt;
};

static void* rtpeak_open(phi_track *t)
{
	struct rtpeak *p = phi_track_allocT(t, struct rtpeak);
	p->fmt = t->audio.format;
	if (0 != pcm_maxpeak(&p->fmt, NULL, 0, NULL)) {
		errlog(t, "pcm_maxpeak(): format not supported");
		phi_track_free(t, p);
		return PHI_OPEN_ERR;
	}
	return p;
}

static void rtpeak_close(void *ctx, phi_track *t)
{
	struct rtpeak *p = ctx;
	phi_track_free(t, p);
}

static int rtpeak_process(void *ctx, phi_track *t)
{
	struct rtpeak *p = ctx;

	double maxpeak = 0;
	pcm_maxpeak(&p->fmt, t->data_in.ptr, t->data_in.len / pcm_size1(&p->fmt), &maxpeak);
	double db = gain_db(maxpeak);
	t->audio.maxpeak_db = db;
	dbglog(t, "maxpeak:%.2F", db);

	t->data_out = t->data_in;
	return (t->chain_flags & PHI_FFIRST) ? PHI_DONE : PHI_OK;
}

const phi_filter phi_rtpeak = {
	rtpeak_open, rtpeak_close, rtpeak_process,
	"rtpeak"
};
