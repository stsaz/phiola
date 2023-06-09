/** phiola: public interface */

#pragma once
#include <FFOS/file.h>
#include <FFOS/timerqueue.h>
#include <ffbase/string.h>

#define PHI_VERSION  20000

/** Inter-module compatibility version.
It must be updated when incompatible changes are made to this file,
 then all modules must be rebuilt.
The core will refuse to load modules built for any other core version. */
#define PHI_VERSION_CORE  20000

typedef ffuint uint;
typedef ffushort ushort;

struct phi_track_if;
typedef struct phi_filter phi_filter;
typedef struct phi_track phi_track;
typedef struct fftask phi_task;
typedef struct fftimerqueue_node phi_timer;

enum PHI_LOG {
	PHI_LOG_ERR,
	PHI_LOG_WARN,
	PHI_LOG_USER,
	PHI_LOG_INFO,
	PHI_LOG_VERBOSE,
	PHI_LOG_DEBUG,
	PHI_LOG_EXTRA,

	PHI_LOG_SYS = 0x10,
};

/** phiola Core.
Usage:
. Executor fills 'phi_core_conf' object and calls phi_core_create()
. Core instance (singleton) is initialized
. Executor uses 'phi_core' interface to initialize some modules
   and 'phi_track_if' interface to start some audio tracks
. Executor calls phi_core_run() to run Core processor
. Executor calls core->sig(PHI_CORE_STOP) to stop Core processor
   and phi_core_destroy() to destroy Core instance
*/
struct phi_core_conf {
	uint log_level; // enum PHI_LOG

	/**
	flags: enum PHI_LOG */
	void (*log)(void *log_obj, uint flags, const char *module, phi_track *trk, const char *fmt, ...);
	void (*logv)(void *log_obj, uint flags, const char *module, phi_track *trk, const char *fmt, va_list va);
	void *log_obj;

	uint code_page; // enum FFUNICODE_CP
	ffstr root; // phiola app directory

	uint timer_interval_msec;
	uint max_tasks; // Max concurrent system tasks
	uint run_detach :1; // phi_core_run() will detach from parent thread
	uint stdin_busy :1; // Prevent TUI module from using stdin
};

enum PHI_CORE_SIG {
	PHI_CORE_STOP,
};

typedef void (*phi_task_func)(void *param);

typedef struct phi_kevent phi_kevent;

typedef struct phi_core phi_core;
struct phi_core {
	const char *version_str;
	struct phi_core_conf conf;
	const struct phi_track_if* track; // track manager interface

	/** Get interface from a module.
	Load module at first use.
	name: "module.interface" */
	const void* (*mod)(const char *name);

	/**
	signal: enum PHI_CORE_SIG */
	void (*sig)(uint signal);

	phi_kevent* (*kev_alloc)();
	void (*kev_free)(phi_kevent *kev);
	int (*kq_attach)(phi_kevent *kev, fffd fd, uint flags);
	void (*timer)(phi_timer *t, int interval_msec, phi_task_func func, void *param);
	void (*task)(phi_task *t, phi_task_func func, void *param);

#ifdef FF_WIN
	int (*woeh)(fffd fd, phi_task *t, phi_task_func func, void *param);
#endif
};

FF_EXTERN phi_core* phi_core_create(struct phi_core_conf *conf);
FF_EXTERN void phi_core_destroy();

/** Run until stopped with core->sig(PHI_CORE_STOP) */
FF_EXTERN void phi_core_run();


/** phiola Module.
Usage:
. A module implements interface 'phi_mod'
   and exports function "phi_mod_init" of type 'phi_mod_init_t'
. Core imports and calls phi_mod_init() when loading a module
   and ensures it's compatible
. Core calls iface() to get a specific interface from a module
. In the end, Core calls close() for each loaded module
*/
typedef struct phi_mod phi_mod;
struct phi_mod {
	uint ver, ver_core;
	const void* (*iface)(const char *name);
	void (*close)(void);
};

typedef const struct phi_mod* (*phi_mod_init_t)(const phi_core *core);


/** Track.
Usage:
. User fills in 'phi_track_conf' object and calls track->create()
. User adds some filters with track->filter()
   and starts the track with track->start()
* Case 1 (stopping an active track):
  . User calls track->stop() to initiate the track's stopping procedure
  . As soon as all filters in chain finish their work, the track object gets destroyed
* Case 2 (stopping a finished track):
  . All filters in chain finish their work
  . User calls track->stop(), and the track object gets destroyed
*/

enum PHI_PCM {
	PHI_PCM_8 = 8,
	PHI_PCM_16 = 16,
	PHI_PCM_24 = 24,
	PHI_PCM_32 = 32,
	PHI_PCM_24_4 = 0x0100 | 32,

	PHI_PCM_FLOAT32 = 0x0200 | 32,
	PHI_PCM_FLOAT64 = 0x0200 | 64,
};

struct phi_af {
	ushort format; // enum PHI_PCM
	u_char channels;
	u_char interleaved :1;
	uint rate;
};

/** Track configuration */
struct phi_track_conf {
	struct {
		char*	name;
		uint	buf_size;
		ffslice	include, exclude; // ffstr[]
		uint	preserve_date :1;
	} ifile;

	uint64	seek_msec;
	uint64	until_msec;

	ffvec	meta; // char*[]

	struct {
		struct phi_af format;
		uint	device_index; // 0:default
		uint	buf_time; // msec
		uint	exclusive :1;
		uint	loopback :1;
		uint	power_save :1;
	} iaudio;

	struct {
		double	gain_db; // Audio gain/attenuation
		uint	peaks_info :1;
		uint	peaks_crc :1;
	} afilter;

	struct {
		char	profile; // LC:'l' | HE:'h' | HEv2:'H'
		ushort	quality; // VBR:1..5 | CBR:8..800
		ushort	bandwidth;
	} aac;

	struct {
		ffbyte	quality; // (q+1.0)*10
	} vorbis;

	struct {
		ushort	bitrate;
		ffbyte	mode; // 0:audio; 1:voip
		ffbyte	bandwidth; // either 4, 6, 8, 12, 20kHz
	} opus;

	struct {
		struct phi_af format;
		uint	device_index; // 0:default
		uint	buf_time; // msec
		uint	exclusive :1;
	} oaudio;

	struct {
		char*	name;
		fftime	mtime;
		uint	buf_size;
		uint	overwrite :1;
		uint	name_tmp :1; // Write data to ".tmp" file, then rename file on completion
	} ofile;

	uint print_time :1;
	uint info_only :1;
	uint print_tags :1;
	uint stream_copy :1;
};

enum PHI_TF {
	PHI_TF_NEXT,
	PHI_TF_PREV,
};

enum PHI_TRACK_CMD {
	PHI_TRACK_STOP_ALL = 1,
	PHI_TRACK_CUR_FILTER_NAME, // Get current filter name
};

typedef struct phi_track_if phi_track_if;
struct phi_track_if {
	/** Create default configuration */
	int (*conf)(struct phi_track_conf *conf);

	phi_track* (*create)(struct phi_track_conf *conf);

	/** Close track (due to an error) before start() is called */
	void (*close)(phi_track *t);

	/** Add filter to the processing chain
	flags: enum PHI_TF */
	int (*filter)(phi_track *t, const phi_filter *f, uint flags);

	void (*start)(phi_track *t);
	void (*stop)(phi_track *t);
	void (*wake)(phi_track *t);

	/**
	cmd: enum PHI_TRACK_CMD */
	ffssize (*cmd)(phi_track *t, uint cmd, ...);
};


enum PHI_META {
	PHI_META_UNIQUE = 1,
};

typedef struct phi_meta_if phi_meta_if;
struct phi_meta_if {

	void (*set)(ffvec *meta, ffstr name, ffstr val);

	int (*find)(const ffvec *meta, ffstr name, ffstr *val, uint flags);

	/**
	flags: enum PHI_META */
	int (*list)(const ffvec *meta, uint *idx, ffstr *name, ffstr *val, uint flags);

	void (*destroy)(ffvec *meta);
};


enum PHI_ADEV_F {
	PHI_ADEV_PLAYBACK,
	PHI_ADEV_CAPTURE,
};

struct phi_adev_ent {
	char *name;
	struct phi_af default_format;
	uint default_device :1;
};

typedef struct phi_adev_if phi_adev_if;
struct phi_adev_if {

	/**
	flags: enum PHI_ADEV_F
	Return N devices */
	int (*list)(struct phi_adev_ent **ents, uint flags);

	void (*list_free)(struct phi_adev_ent *ents);
};


struct phi_queue_conf {
	const char *name;
	const phi_filter *first_filter;
	const char *audio_module;
	uint conversion :1;
	uint random :1;
	uint repeat_all :1;
};

struct phi_queue_entry {
	struct phi_track_conf conf;
	uint length_msec;
};

typedef struct phi_queue* phi_queue_id;
typedef struct phi_queue_if phi_queue_if;
struct phi_queue_if {
	phi_queue_id (*create)(struct phi_queue_conf *conf);
	void (*destroy)(phi_queue_id q);
	int (*add)(phi_queue_id q, struct phi_queue_entry *qe);
	int (*clear)(phi_queue_id q);
	int (*count)(phi_queue_id q);

	int (*play)(phi_queue_id q, void *e);
	int (*play_next)(phi_queue_id q);
	int (*play_previous)(phi_queue_id q);

	int (*save)(phi_queue_id q, const char *filename);
	int (*status)(phi_queue_id q);

	struct phi_queue_entry* (*at)(phi_queue_id q, uint pos);
	int (*remove_at)(phi_queue_id q, uint pos, uint n);

	void* (*insert)(void *e, struct phi_queue_entry *qe);
	struct phi_queue_conf* (*conf)(void *e);
	int (*index)(void *e);
	int (*remove)(void *e);
};
