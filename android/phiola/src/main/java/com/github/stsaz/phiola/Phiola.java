/** phiola/Android: libphiola.so interface
2022, Simon Zolin */

package com.github.stsaz.phiola;

class Phiola {
	Phiola(Object asset_mgr) {
		System.loadLibrary("phiola");
		init(asset_mgr);
	}
	private native void init(Object asset_mgr);

	/** Close libphiola. */
	native void destroy();

	private static boolean lib_load(String name) {
		System.loadLibrary(name);
		return true;
	}

	/** Get libphiola version. */
	native String version();

	/** Enable or disable debug logging. */
	native void setDebug(boolean enable);

	static class Config {
		String	codepage;
		String	equalizer;
		int		queue_flags; // QC_*
		int		auto_seek;
		int		auto_until;
		boolean	deprecated_mods;
	}

	/** Apply new settings. */
	native void setConfig(int flags, Config conf);

	interface Callbacks {
		/** Notifies when a new track starts playing. */
		void play_new(Meta meta);

		/** Notifies when playback finishes.
		status: PCS_* */
		void play_fin(int status);

		void play_update(long pos_msec);

		/** Notifies when recording is finished.
		mode: 0:mic; 1:radio
		code: enum PHI_E */
		void recording(int mode, int code, String filename);
	}

	/** Set the playback/recording callbacks. */
	native void setCallbacks(Callbacks cb);

	static class Meta {
		int queue_pos;
		long length_msec;
		String url, artist, title, album, date, info;
		String[] meta;
		static final int N_RESERVED = 5; // URL, SIZE, MTIME, LENGTH, FORMAT
	}

	/** Status flag for Callbacks.play_fin(). */
	static final int
		/** Playback ended by explicit stop. */
		PCS_STOP = 1,

		/** Playback ended by auto-stop. */
		PCS_AUTOSTOP = 2;

	/** Playback command IDs for playCmd(). */
	static final int
		/** Toggle pause/resume playback. */
		PC_PAUSE_TOGGLE = 1,

		/** Stop playback. */
		PC_STOP = 2,

		/** Toggle auto-stop (val: timeout in msec). */
		PC_AUTO_STOP = 5,

		/** Seek to position (val: seek position in msec). */
		PC_SEEK = 6;

	/** Execute a playback command. */
	native void playCmd(int cmd, long val);

	/** Start recording from radio. */
	native void playRecord(String oname); // -> Callbacks.recording()

	static final int
		AF_AAC_LC = 0,
		AF_AAC_HE = 1,
		AF_AAC_HE2 = 2,
		AF_FLAC = 3,
		AF_OPUS = 4,
		AF_OPUS_VOICE = 5,
		AF_MP3 = 6,
		AF_WAV = 7,
		AF_FLAC24 = 8;

	// enum PHI_PCM
	static final int
		SF_INT16 = 16,
		SF_INT24 = 24,
		SF_INT32 = 32,
		SF_FLOAT32 = 32 | 0x0100;

	static class ConvertParams {
		ConvertParams() {
			out_name = "";
			from_msec = "";
			to_msec = "";
			tags = "";
			trash_dir_rel = "";
			q_pos = -1;
		}

		int sample_format;
		int sample_rate;

		int format; // AF_*
		int aac_quality;
		int opus_quality;
		int mp3_quality;

		static final int
			COF_DATE_PRESERVE = 1,
			COF_OVERWRITE = 2,
			COF_COPY = 4,
			COF_ADD = 8; // add item to playlist `q_add_remove`
		int flags;

		String out_name;
		String from_msec, to_msec;
		String tags;
		long q_add_remove;
		int q_pos;
		String trash_dir_rel;
	}

	// RECORD

	static class RecordParams {
		int format; // AF_*
		int quality;

		int channels;
		int sample_rate;
		int sample_format; // SF_*

		static final int
			RECF_EXCLUSIVE = 1,
			RECF_POWER_SAVE = 2,
			RECF_DANORM = 8;
		int flags;
		String src_preset = "";

		int buf_len_msec;
		int gain_db100;
		int until_sec;
	}

	/** Start recording. */
	native long recStart(String oname, RecordParams conf); // -> Callbacks.recording()

	/** Recording command IDs for recCtrl(). */
	static final int
		/** Stop recording. */
		RECL_STOP = 1,

		/** Pause recording. */
		RECL_PAUSE = 2,

		/** Resume recording. */
		RECL_RESUME = 3;

	/** Control an active recording. */
	native String recCtrl(long trk, int cmd);

	static class RecInfo {
		int sec;
		double cur_db, max_db;
	}

	/** Return information about an active recording. */
	native RecInfo recInfo(long trk);

	// TAGS EDIT

	static final int
		TE_CLEAR = 1,
		TE_PRESERVE_DATE = 2;

	/** Edit file tags. */
	native int tagsEdit(String filename, String[] tags, int flags);

	// track queue

	interface QueueCallback {
		void on_change(long q, int flags, int pos);
		void on_complete(int operation, int status);
	}

	/** Set the callback for queue events. */
	native void quSetCallback(QueueCallback cb);

	static final int
		QUNF_CONVERSION = 1;

	/** Create a new queue. */
	native long quNew(int flags);

	/** Destroy a queue. */
	native void quDestroy(long q);

	/** Move the list to a new position. */
	native void quMove(int from, int to);

	/** Copy entries from another queue (asynchronous).
	pos: Source entry index
		-1: Copy all entries */
	native void quDup(long q, long q_src, int pos);

	static final int QUADD_RECURSE = 1;

	/** Add entries to a queue. */
	native void quAdd(long q, String[] urls, int flags);

	/** Get an entry from a queue. */
	native String quEntry(long q, int i);

	/** Queue command IDs for quCmd(). */
	static final int
		/** Clear all entries. */
		QUCOM_CLEAR = 1,

		/** Remove a single entry at the specified index. */
		QUCOM_REMOVE_I = 2,

		/** Return the number of entries. */
		QUCOM_COUNT = 3,

		/** Convert track index in the currently visible (filtered) list
		 to the index within its parent (not filtered) list */
		QUCOM_INDEX = 4,

		/** Sort the entries. */
		QUCOM_SORT = 5,

			/** Sort the entries by filename alphabetically. */
			QU_SORT_FILENAME = 0,

			/** Sort the entries by file size. */
			QU_SORT_FILESIZE = 1,

			/** Sort the entries by file modification date. */
			QU_SORT_FILEDATE = 2,

			/** Shuffle the entries randomly. */
			QU_SORT_RANDOM = 3,

			/** Sort the entries by the artist metadata tag. */
			QU_SORT_TAG_ARTIST = 4,

			/** Sort the entries by the date/year metadata tag. */
			QU_SORT_TAG_DATE = 5,

		/** Remove the entries whose files no longer exist on disk. */
		QUCOM_REMOVE_NON_EXISTING = 6,

		/** Play the entry at the specified index. */
		QUCOM_PLAY = 7,

		/** Play the next track. */
		QUCOM_PLAY_NEXT = 8,

		/** Play the previous track. */
		QUCOM_PLAY_PREV = 9,

		/** Read metadata for all entries. */
		QUCOM_META_READ = 10,

		/** Remove duplicate entries. */
		QUCOM_REMOVE_NON_UNIQUE = 11,

		/** Cancel an ongoing audio conversion operation. */
		QUCOM_CONV_CANCEL = 13,

		/** Update current status of all entries.
		Return 0 when conversion is complete. */
		QUCOM_CONV_UPDATE = 14;

	/** Execute a command on a queue or queue entry. */
	native int quCmd(long q, int cmd, int i);

	/** Queue playback flags for Config.queue_flags. */
	static final int
		/** Enable repeat-all mode (repeat the entire playlist). */
		QC_REPEAT = 1,

		/** Enable shuffle/random play order. */
		QC_RANDOM = 2,

		/** Remove the current entry from the queue if playback fails. */
		QC_REMOVE_ON_ERROR = 4,

		/** Enable automatic volume normalization. */
		QC_AUTO_NORM = 0x10,

		/** Enable ReplayGain normalization. */
		QC_RG_NORM = 0x20;

	/** Get metadata for a queue entry. */
	native Meta quMeta(long q, int i);

	/** Move all files in the list to the specified directory.
	Note: the URLs are NOT updated.
	Return N of files moved;
		<0 if some files were not moved. */
	native int quMoveAll(long q, String dst_dir);

	/** Flags for quRename(). */
	static final int
		/** Move a queue entry's file to the specified target directory (preserves filename). */
		QR_MOVE = 0,

		/** Rename a queue entry's file within its current directory (preserves extension). */
		QR_RENAME = 1;

	/** Move file */
	native int quRename(long q, int pos, String target, int flags);

	/** Filter flags for quFilter() to specify what to match against. */
	static final int
		/** Match entries by URL (file path). */
		QUFILTER_URL = 1,

		/** Match entries by metadata (tag values). */
		QUFILTER_META = 2;

	/** Filter the entries in a queue. */
	native long quFilter(long q, String filter, int flags);

	/** Start conversion of the entries in a queue. */
	native String quConvertBegin(long q, ConvertParams conf);

	/** Get the text to display. */
	native String[] quDisplayLine(long q, int i, int n);

	/** Load playlist from a file on disk */
	native int quLoad(long q, String filepath);

	/** Save a queue to a file. */
	native void quSave(long q, String filepath);
}
