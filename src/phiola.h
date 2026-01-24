/** phiola: public interface */

#pragma once
#include <ffsys/error.h>
#include <ffbase/vector.h>
#include <ffbase/string.h>
#include <ffbase/time.h>

#define PHI_VERSION  20702

/** Inter-module compatibility version.
It must be updated when incompatible changes are made to this file,
 then all modules must be rebuilt.
The core will refuse to load modules built for any other core version. */
#define PHI_VERSION_CORE  20702

typedef long long int64;
typedef unsigned long long uint64;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char u_char;

#define PHI_ASSERT  assert

struct phi_track_if;
typedef struct phi_filter phi_filter;
typedef struct phi_track phi_track;
struct _phi_fftask { size_t a[4]; };
typedef struct _phi_fftask phi_task;
struct _phi_fftimerqueue_node { size_t a[8]; };
typedef struct _phi_fftimerqueue_node phi_timer;
struct phi_meta_if;
typedef struct { char data[56]; } phi_meta;

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

	/** Expand system environment variables.
	* UNIX: "text $VAR text"
	* Windows: "text %VAR% text"
	Return newly allocated string; must free with ffmem_free() */
	char* (*env_expand)(const char *s);

	/** Called before loading a module.
	Return newly allocated file name or NULL */
	char* (*mod_loading)(ffstr name);

	/** Get data from resource file. */
	ffstr (*resource_load)(const char *name);

	char language[2];
	uint code_page; // enum FFUNICODE_CP
	ffstr root; // phiola app directory

	/** Use up to N worker threads
	0: 1 worker
	-1: all available CPU */
	uint workers;

	/** Worker-to-CPU affinity.
	e.g. `01010101` for 4 workers -> use cores 0,2,4,6. */
	uint cpu_affinity;

	/** Number of I/O workers.
	-1: ==workers */
	uint io_workers;

	uint timer_interval_msec;
	uint max_tasks; // Max concurrent system tasks
	uint run_detach :1; // phi_core_run() will detach from parent thread
	uint stdin_busy :1; // Prevent TUI module from using stdin
	uint stdout_busy :1; // Prevent TUI module from using stdout
};

enum PHI_CORE_TIME {
	PHI_CORE_TIME_UTC,
	PHI_CORE_TIME_LOCAL,
	PHI_CORE_TIME_MONOTONIC,
};

enum PHI_CORE_SIG {
	PHI_CORE_STOP,
};

typedef void (*phi_task_func)(void *param);

typedef struct phi_kevent phi_kevent;

struct phi_woeh_task {
	phi_task task;
	uint worker;
};

typedef struct phi_core phi_core;
struct phi_core {
	const char *version_str;
	struct phi_core_conf conf;
	const struct phi_track_if* track; // track manager interface
	const struct phi_meta_if* metaif;

	/**
	flags: enum PHI_CORE_TIME */
	fftime (*time)(ffdatetime *dt, uint flags);

	/** Get interface from a module.
	Load module at first use.
	name: "module.interface" */
	const void* (*mod)(const char *name);

	/**
	signal: enum PHI_CORE_SIG */
	void (*sig)(uint signal);

	phi_kevent* (*kev_alloc)(uint worker);
	void (*kev_free)(uint worker, phi_kevent *kev);
	int (*kq_attach)(uint worker, phi_kevent *kev, fffd fd, uint flags);

	/** Start/stop a oneshot/periodic timer.
	worker: worker ID, either returned by worker_assign() or 0 for main worker.
		The function MUST be called from the same worker.
	interval_msec:
		<0: one shot timer;
		>0: periodic timer;
		=0: stop timer */
	void (*timer)(uint worker, phi_timer *t, int interval_msec, phi_task_func func, void *param);

	/** Add/remove an asynchronous task to a worker's queue.
	func: task handling function;
		NULL: remove the previously added task from queue */
	void (*task)(uint worker, phi_task *t, phi_task_func func, void *param);

#ifdef FF_WIN
	/** Set the function to receive signals from a Windows event handle.
	User function is called inside the specified worker thread.
	flags: 1:one-shot */
	int (*woeh)(uint worker, fffd fd, struct phi_woeh_task *t, phi_task_func func, void *param, uint flags);
#endif

	/** Get the number of available workers */
	int (*workers_available)();

	/** Assign a new task to a worker.
	flags:
		1: cross-worker assignment (for conversion tracks only)
	Return worker ID */
	uint (*worker_assign)(uint flags);

	/** Unassign a task from a worker */
	void (*worker_release)(uint worker);
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

/** Sample formats (must match enum FFAUDIO_F) */
enum PHI_PCM {
	PHI_PCM_8 = 8,
	PHI_PCM_16 = 16,
	PHI_PCM_24 = 24,
	PHI_PCM_32 = 32,
	PHI_PCM_24_4 = 32 | 0x0200,

	PHI_PCM_U8 = 8 | 0x0400,

	PHI_PCM_FLOAT32 = 32 | 0x0100,
	PHI_PCM_FLOAT64 = 64 | 0x0100,
};

/** Audio format (must match struct pcm_af) */
struct phi_af {
	ushort format; // enum PHI_PCM
	u_char channels;
	u_char interleaved :1;
	uint rate;
};

enum PHI_AC {
	PHI_AC_AAC = 1,
	PHI_AC_FLAC,
	PHI_AC_MP3,
	PHI_AC_OPUS,
	PHI_AC_VORBIS,
	PHI_AC_WAV,
};

enum PHI_CUE_GAP {
	/** Add gap to the end of the previous track:
	track01.index01 .. track02.index01 */
	PHI_CUE_GAP_PREV,

	/** Add gap to the end of the previous track (but track01's pregap is preserved):
	track01.index00 .. track02.index01
	track02.index01 .. track03.index01 */
	PHI_CUE_GAP_PREV1,

	/** Add gap to the beginning of the current track:
	track01.index00 .. track02.index00 */
	PHI_CUE_GAP_CURR,

	/** Skip pregaps:
	track01.index01 .. track02.index00 */
	PHI_CUE_GAP_SKIP,
};

/** Track configuration */
struct phi_track_conf {
	struct {
		char*	name;
		uint	buf_size;
		ffslice	include, exclude; // ffstr[]
		u_char	connect_timeout_sec;
		u_char	recv_timeout_sec;
		u_char	format; // enum AVPK_FORMAT
		uint	preserve_date :1;
		uint	no_meta :1;
	} ifile;

	ffslice tracks; // uint[]

	uint	split_msec;
	uint64	seek_msec, until_msec;
	uint	seek_cdframes, until_cdframes;

	phi_meta	meta;

	const char*	tee; // Name of the file where input data will be copied

	struct {
		struct phi_af format;
		union {
		uint	device_index; // 0:default
		size_t	device_id;
		};
		uint	buf_time; // msec
		uint	exclusive :1;
		uint	loopback :1;
		uint	power_save :1;
	} iaudio;

	struct {
		double	gain_db; // Audio gain/attenuation
		uint	rg_normalizer :1;
		uint	peaks_info :1;
		uint	loudness_summary :1;
		const char *auto_normalizer;
		const char *danorm;
		const char *noise_gate;
		const char *equalizer;
	} afilter;

	// Audio encoder selected by `ofile.name`
	union {
		struct {
			char data[6];
		} encoder;

		struct {
			char	profile; // LC:'l' | HE:'h' | HEv2:'H'
			ushort	quality; // VBR:1..5 | CBR:8..800
			ushort	bandwidth;
		} aac;

		struct {
			u_char	quality; // (q+1.0)*10
		} vorbis;

		struct {
			ushort	quality; // +1
		} mp3;

		struct {
			ushort	bitrate;
			u_char	mode; // 0:audio; 1:voip
			u_char	bandwidth; // either 4, 6, 8, 12, 20kHz
		} opus;
	};

	struct {
		u_char	max_page_length_msec;
	} ogg;

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

	u_char	cue_gaps; // enum PHI_CUE_GAP
	uint	print_time :1;
	uint	info_only :1;
	uint	print_tags :1;
	uint	stream_copy :1;
	uint	cross_worker_assign :1;
	uint	tee_output :1; // `tee` is the file name for *output* data, not *input* data
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

	void* (*memalloc)(phi_track *t, uint n);
	void (*memfree)(phi_track *t, void *ptr);

	/**
	cmd: enum PHI_TRACK_CMD */
	ffssize (*cmd)(phi_track *t, uint cmd, ...);
};


enum PHI_META_LIST {
	PHI_META_UNIQUE = 1,	// Exclude entries with the same key
	PHI_META_PRIVATE = 2,	// Include private entries starting with "_phi_"
};

enum PHI_META_SET {
	// PHI_META_UNIQUE = 1	// Do nothing if the key exists
	PHI_META_REPLACE = 4,	// Replace existing key-value pair
	PHI_META_CACHE = 8		// Store data in cache (if it fits)
};

typedef struct phi_meta_if phi_meta_if;
struct phi_meta_if {

	/** Set meta data.
	flags: enum PHI_META_SET */
	void (*set)(phi_meta *meta, ffstr name, ffstr val, uint flags);

	/**
	flags: enum PHI_META_SET */
	void (*copy)(phi_meta *dst, const phi_meta *src, uint flags);

	/**
	Return 0 on success */
	int (*find)(const phi_meta *meta, ffstr name, ffstr *val, uint flags);

	/**
	idx: must be initialized to 0
	flags: enum PHI_META_LIST
	Return 0 on complete */
	int (*list)(const phi_meta *meta, uint *idx, ffstr *name, ffstr *val, uint flags);

	void (*destroy)(phi_meta *meta);
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
	char *name;
	const phi_filter *first_filter;
	const char *audio_module;
	union {
		const char *ui_module;
		const phi_filter *ui_module_if;
	};
	struct phi_track_conf tconf;
	fftime last_mod_time;
	uint conversion :1;
	uint analyze :1;
	uint random :1;
	uint repeat_all :1;
	uint modified :1;
	uint ui_module_if_set :1;
};

struct phi_queue_entry {
	char*	url;
	phi_meta meta;
	uint	length_sec :24;
	uint	meta_priority :1; // Supplied `meta` has higher priority than meta from input file (e.g. for .cue track)
	uint	lock; // For synchronizing access to `meta`
	uint	seek_cdframes, until_cdframes;
};

#define PHI_Q_PLAY_NEXT  ((void*)1)
#define PHI_Q_PLAY_PREVIOUS  ((void*)-1)

enum PHI_Q_SORT {
	PHI_Q_SORT_FILENAME,
	PHI_Q_SORT_FILESIZE,
	PHI_Q_SORT_FILEDATE,
	PHI_Q_SORT_RANDOM,
	PHI_Q_SORT_TAG_ARTIST,
	PHI_Q_SORT_TAG_DATE,
};

enum PHI_Q_REMOVE {
	PHI_Q_RM_NONEXIST = 1,
};

enum PHI_QUEUE_FILTER {
	PHI_QF_FILENAME = 1,
	PHI_QF_META = 2,
};

enum PHI_Q_RENAME {
	PHI_QRN_ACQUIRE = 1,
};

typedef struct phi_queue* phi_queue_id;
typedef struct phi_queue_if phi_queue_if;
struct phi_queue_if {
	/** Assign callback function to receive events from queue module.
	cb.flags:
		'a': item @pos added
		'r': item @pos removed
		'm': updated meta for the item @pos
		'n': list created
		'c': cleared
		'd': deleted
		'u': updated
		'.': reached the end
	*/
	void (*on_change)(void (*cb)(phi_queue_id q, uint flags, uint pos));

	phi_queue_id (*create)(struct phi_queue_conf *conf);
	void (*destroy)(phi_queue_id q);
	phi_queue_id (*select)(uint pos);
	struct phi_queue_conf* (*conf)(phi_queue_id q);
	void (*qselect)(phi_queue_id q);

	/** Move list to a new position. */
	void (*move)(uint from, uint to);

	int (*add)(phi_queue_id q, struct phi_queue_entry *qe);
	int (*count)(phi_queue_id q);

	/** Create a new virtual queue with the items matching a filter
	flags: enum PHI_QUEUE_FILTER */
	phi_queue_id (*filter)(phi_queue_id q, ffstr filter, uint flags);

	/** e: queue track pointer or PHI_Q_PLAY_* value */
	int (*play)(phi_queue_id q, void *e);

	int (*save)(phi_queue_id q, const char *filename, void (*on_complete)(void*, phi_track*), void *param);
	int (*status)(phi_queue_id q);

	/**
	Generates on_change('u') event.
	flags: enum PHI_Q_SORT */
	void (*sort)(phi_queue_id q, uint flags);

	/** Read meta data of all tracks.
	Generates on_change('m') event. */
	void (*read_meta)(phi_queue_id q);

	/** Rename all files. */
	int (*rename_all)(phi_queue_id q, const char *pattern, uint flags);

	/** Remove all items.
	Generates on_change('c') event. */
	int (*clear)(phi_queue_id q);

	/** Remove items at position.
	Generates on_change('r') event. */
	int (*remove_at)(phi_queue_id q, uint pos, uint n);

	/** Remove items.
	Generates on_change('u') event.
	flags: enum PHI_Q_REMOVE */
	void (*remove_multi)(phi_queue_id q, uint flags);

	struct phi_queue_entry* (*at)(phi_queue_id q, uint pos);

	/** Get item pointer, increase refcount.
	Guarantees that the returned item won't be suddenly destroyed by remove() from the main thread.
	Each ref() must be paired with unref(). */
	struct phi_queue_entry* (*ref)(phi_queue_id q, uint pos);

	int (*ref_bulk)(phi_queue_id q, uint pos, uint n, struct phi_queue_entry **result);

	/** Decrease refcount for the item obtained by ref(). */
	void (*unref)(struct phi_queue_entry *qe);

	/** Get the queue containing this item */
	phi_queue_id (*queue)(void *e);

	void* (*insert)(void *e, struct phi_queue_entry *qe);
	void* (*insert_bulk)(void *e, struct phi_queue_entry *qe, uint n, struct phi_queue_entry **result);
	int (*index)(void *e);

	/** Remove item.
	Generates on_change('r') event. */
	int (*remove)(void *e);

	/** Rename item's source file.
	flags: enum PHI_Q_RENAME */
	int (*rename)(struct phi_queue_entry *qe, char *new_url, uint flags);
};


/** Remote control */

typedef struct phi_remote_sv_if phi_remote_sv_if;
struct phi_remote_sv_if {
	int (*start)(const char *name);
};

enum PHI_RCLF {
	PHI_RCLF_NOLOG = 1,
};

typedef struct phi_remote_cl_if phi_remote_cl_if;
struct phi_remote_cl_if {
	int (*cmd)(const char *name, ffstr cmd);

	/**
	names: char*[]
	flags: enum PHI_RCLF */
	int (*play)(const char *name, ffslice names, uint flags);
};


/** Modify meta tags */

struct phi_tag_conf {
	const char *filename;
	ffslice meta; // ffstr[]{name=value}
	uint clear :1;
	uint preserve_date :1;
	uint no_expand :1;
};

typedef struct phi_tag_if phi_tag_if;
struct phi_tag_if {
	int (*edit)(struct phi_tag_conf *conf);
};


/** UI configuration */

typedef void (*phi_log_ctl)(uint flags);
struct phi_ui_conf {
	u_char volume_percent;
	phi_log_ctl log_ctl;
};

enum PHI_UI_SEEK {
	PHI_UI_SEEK_FWD,
	PHI_UI_SEEK_BACK,
};

typedef struct phi_ui_if phi_ui_if;
struct phi_ui_if {
	void (*conf)(struct phi_ui_conf *c);
	void (*log)(void *udata, ffstr s);

	/**
	flags: enum PHI_UI_SEEK */
	void (*seek)(uint val, uint flags);
};


/** Audio Streaming Server */

struct phi_asv_conf {
	uint max_clients;
	ushort port;
};
