/** phiola: example recorder */

#include <track.h>
#include <FFOS/ffos-extern.h>

const phi_core *core;
const phi_track_if *track;

void phi_rec_close(void *f, phi_track *t)
{
	core->sig(PHI_CORE_STOP);
}

int phi_rec_process(void *f, phi_track *t) { return PHI_DONE; }

phi_filter phi_example_rec = {
	NULL, phi_rec_close, phi_rec_process,
	"example-record"
};

void record(const char *fn)
{
	struct phi_track_conf c = {
		.audio_in.format = {
			.format = 24,
			.channels = 2,
			.sample_rate = 48000,
		},
		.aac = {
			.profile = 'H',
			.quality = 64,
		},
		.ofile = {
			.name = ffsz_dup(fn),
			.seekable = 1,
			.overwrite = 1,
		},
	};

	phi_track *t = track->create(&c);

	track->filter(t, &phi_example_rec, 0);
	track->filter(t, core->mod("core.auto-rec"), 0);
	track->filter(t, core->mod("afilter.auto-conv"), 0);
	track->filter(t, core->mod("format.auto-write"), 0);
	track->filter(t, core->mod("core.file-write"), 0);

	const phi_meta_if *meta = core->mod("format.meta");
	meta->set(&t->meta, FFSTR_Z("artist"), FFSTR_Z("Example Artist"));
	meta->set(&t->meta, FFSTR_Z("title"), FFSTR_Z("Example Title"));

	track->start(t);
}

void on_sig(struct ffsig_info *i)
{
	track->cmd(NULL, PHI_TRACK_STOP_ALL);
}

int main(int argc, char **argv, char **env)
{
	uint sigs[] = { SIGINT };
	ffsig_subscribe(on_sig, sigs, 1);

	struct phi_core_conf conf = {
		.log_level = PHI_LOG_INFO,
	};
	core = phi_core_create(&conf);
	track = core->track;

	record("rec.m4a");

	phi_core_run();
	phi_core_destroy();
}
