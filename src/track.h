/** phiola: Track internals */

#pragma once
#include <phiola.h>
#include <util/taskqueue.h>
#include <ffsys/time.h>
#include <ffsys/kcall.h>
#include <ffbase/list.h>


#define phi_syserrlog(core, mod, trk, ...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR | PHI_LOG_SYS, mod, trk, __VA_ARGS__)

#define phi_errlog(core, mod, trk, ...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_ERR, mod, trk, __VA_ARGS__)

#define phi_syswarnlog(core, mod, trk, ...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_WARN | PHI_LOG_SYS, mod, trk, __VA_ARGS__)

#define phi_warnlog(core, mod, trk, ...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_WARN, mod, trk, __VA_ARGS__)

#define phi_userlog(core, mod, trk, ...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_USER, mod, trk, __VA_ARGS__)

#define phi_infolog(core, mod, trk, ...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_INFO, mod, trk, __VA_ARGS__)

#define phi_verblog(core, mod, trk, ...) \
	core->conf.log(core->conf.log_obj, PHI_LOG_VERBOSE, mod, trk, __VA_ARGS__)

#define phi_dbglogv(core, mod, trk, fmt, va) \
do { \
	if (ff_unlikely(core->conf.log_level >= PHI_LOG_DEBUG)) \
		core->conf.logv(core->conf.log_obj, PHI_LOG_DEBUG, mod, trk, fmt, va); \
} while (0)

#define phi_dbglog(core, mod, trk, ...) \
do { \
	if (ff_unlikely(core->conf.log_level >= PHI_LOG_DEBUG)) \
		core->conf.log(core->conf.log_obj, PHI_LOG_DEBUG, mod, trk, __VA_ARGS__); \
} while (0)

#define phi_extralog(core, mod, trk, ...) \
do { \
	if (ff_unlikely(core->conf.log_level >= PHI_LOG_EXTRA)) \
		core->conf.log(core->conf.log_obj, PHI_LOG_EXTRA, mod, trk, __VA_ARGS__); \
} while (0)


struct phi_kevent {
	phi_task_func rhandler, whandler;
	union {
		ffkq_task rtask;
		ffkq_task_accept rtask_accept;
	};
	ffkq_task wtask;
	uint side;
	void *obj;
	struct phi_kevent *prev_kev;
	struct ffkcall kcall;
};


#define PHI_OPEN_SKIP  NULL // same as PHI_DONE
#define PHI_OPEN_ERR  (void*)-1 // same as PHI_ERR

enum PHI_R {
	// Go forward through the chain with a chunk of output data inside 'phi_track.data_out'.
	// The output data region must stay valid until the filter is called again.
	PHI_DATA,
	PHI_OK,		// the filter will be called next time with more input data only
	PHI_DONE,	// remove this filter from the chain
	PHI_LASTOUT,// remove this & all previous modules from the chain

	// Go back through the chain and return with more input data inside 'phi_track.data_in'.
	// The input data region is guaranteed to be valid until the filter asks for more.
	PHI_MORE,	// need more input data from the previous filter
	PHI_BACK,	// pass the current output data to the previous filter

	PHI_ASYNC,	// the filter suspends the track execution; the filter will call track->wake() to continue
	PHI_FIN,	// don't call any more filters and close the track
	PHI_ERR,	// error, set 'phi_track.error' if not set, then close the track
};

struct phi_filter {

	/** Open filter.
	Return filter instance for this track or PHI_OPEN_... */
	void* (*open)(phi_track *t);

	void (*close)(void *f, phi_track *t);

	/** Return enum PHI_R */
	int (*process)(void *f, phi_track *t);

	char name[16];
};

#define MAX_FILTERS 24

struct filter {
	void *obj;
	int (*process)(void *obj, phi_track *t);
	const struct phi_filter *iface;

	uint64 busytime_nsec :63;
	uint64 backward_skip :1;
};

struct phi_conveyor {
	struct filter filters_pool[MAX_FILTERS];
	u_char filters_active[MAX_FILTERS];
	uint i_fpool, n_active, cur;
};

enum PHI_F {
	PHI_FFIRST = 1, // filter is first in chain
	PHI_FSTOP = 2, // track is being stopped
	PHI_FFWD = 4, // going forward through the chain
	PHI_FFINISHED = 8, // the chain is finished either by PHI_DONE/PHI_FIN or PHI_ERR
};

enum PHI_E {
	PHI_E_OK,
	PHI_E_NOSRC, // no source
	PHI_E_DSTEXIST, // target exists already
	PHI_E_UNKIFMT, // unknown input format
	PHI_E_AUDIO_INPUT,
	PHI_E_CANCELLED,
	PHI_E_ACONV, // audio conversion
	PHI_E_OUT_FMT,
	PHI_E_OTHER = 255,
	PHI_E_SYS = 0x80000000,
};

/** Track instance */
struct phi_track {
	ffchain_item sib;
	uint worker;
	phi_task task_wake, task_stop;
	struct phi_conveyor conveyor;
	fftime t_start;
	uint error; // enum PHI_E
	uint state; // enum STATE
	char id[12];

	struct phi_track_conf conf;
	uint chain_flags; // enum PHI_F
	ffstr data_in, data_out;
	const char *data_type;

	struct {
		uint64 size; // Input file size. -1:unset
		uint64 seek; // Seek to offset and reset. -1:unset
		fftime mtime; // Modification date/time
	} input;

	phi_meta meta;
	void *qent;
	void *udata;
	uint icy_meta_interval; // Upon receiving HTTP response, 'http' filter sets ICY meta interval for 'icy' filter
	uint meta_changed :1; // Set by 'icy' filter when meta is changed; reset by 'ui' filter

	struct {
		struct phi_af format;
		uint64	pos, total; // samples; -1:unset/unknown
		int64	seek; // >0:msec; -1:unset
		uint	seek_req :1; // New seek request is received (UI -> fmt.read)
		uint	ogg_reset :1; // ogg.read -> opus.meta
		uint	bitrate;
		double	maxpeak_db;
		const char *decoder;

		// Set by mp4.read, mp3.read, opus.dec
		uint start_delay, end_padding;

		union {
			// flac.read/ogg -> flac.dec
			struct {
				uint flac_samples;
				uint flac_minblock, flac_maxblock;
			};

			// ape.read -> ape.dec
			struct {
				uint ape_block_samples;
				uint ape_align4;
			};

			// mp3.read -> mpeg.dec
			u_char mpeg1_vbr_scale; // +1
		};
	} audio;

	struct {
		uint width, height;
		const char *decoder;
	} video;

	// afilter.conv settings
	// Note: reused each time afilter.conv is added
	struct {
		struct phi_af in, out;
	} aconv;

	union {
		struct {
			struct phi_af format;
			struct phi_af conv_format;
			double gain_db;
			double loudness, loudness_momentary;

			// ui -> audio.play
			void *adev_ctx;
			void (*adev_stop)(void *adev_ctx);

			// (aac.read|aac.enc) -> mp4.write
			uint mp4_delay;
			uint mp4_bitrate;

			// ((mp4|mkv|aac).read|aac.enc) -> mp4.write
			uint mp4_frame_samples;

			// flac.enc -> flac.write
			const char *flac_vendor;
			uint flac_frame_samples;

			// (ogg|mkv).read -> ogg.write
			uint64 ogg_granule_pos; // stream_copy=1: granule-position value from source
			uint ogg_flush :1;
			uint ogg_gen_opus_tag :1; // ogg.write must generate Opus-tag packet

			/** Order AO filter to pause playing, then just wait until the track is woken up.
			After the flag is set, at some point AO will see it and reset. */
			uint pause :1;

			uint clear :1;
		} oaudio;

		struct {
			void (*on_complete)(void*, phi_track*);
			void *param;
		} q_save;
	};

	struct {
		char *name;
		uint64 seek; // Seek to offset and reset. -1:unset
		uint cant_seek :1;
		uint allow_async :1;
	} output;

	uint area_cap, area_size;
	u_char area[0];
	// filter private data[]
	// padding[]
	// ...
};

static inline void* phi_track_alloc(phi_track *t, uint n)
{
	uint sz = t->area_size + ffint_align_ceil2(n, 8);
	if (sz > t->area_cap) {
		return ffmem_calloc(1, n);
	}
	void *p = t->area + t->area_size;
	t->area_size = sz;
	// ffmem_zero(p, n);
	return p;
}

#define phi_track_allocT(t, T)  phi_track_alloc(t, sizeof(T))

static inline void phi_track_free(phi_track *t, void *ptr)
{
	if ((u_char*)ptr >= t->area
		&& (u_char*)ptr < t->area + t->area_cap) {
		return;
	}
	ffmem_free(ptr);
}
