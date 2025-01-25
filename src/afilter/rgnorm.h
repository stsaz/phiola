/** phiola: replay-gain normalizer
2025, Simon Zolin */

#include <track.h>
#include <afilter/pcm.h>

static void* rgnorm_open(phi_track *t)
{
	double db;
	ffstr val;
	if (!core->metaif->find(&t->meta, FFSTR_Z("r128_track_gain"), &val, 0)) {
		int n;
		if (!ffstr_to_int32(&val, &n)
			|| Q78_float(n, &db))
			goto end;
		db = RG_from_R128(db);

	} else if (!core->metaif->find(&t->meta, FFSTR_Z("replaygain_track_gain"), &val, 0)) {
		ffstr_trimwhite(&val);
		ffstr_splitby(&val, ' ', &val, NULL);
		if (!ffstr_to_float(&val, &db))
			goto end;

	} else {
		goto end;
	}

	dbglog(t, "replay gain: %f", db);
	t->oaudio.replay_gain_db = db;

end:
	return PHI_OPEN_SKIP;
}

static const phi_filter phi_rg_norm = {
	rgnorm_open, NULL, NULL,
	"replaygain"
};
