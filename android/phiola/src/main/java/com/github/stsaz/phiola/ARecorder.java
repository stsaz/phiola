/** phiola/Android: a wrapper around system MediaRecorder
2024, Simon Zolin */

package com.github.stsaz.phiola;

import android.media.MediaRecorder;

import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;

class ARecorder {
	private static final String TAG = "phiola.ARecorder";
	private Core core;
	private MediaRecorder mr;
	private String out;

	ARecorder(Core core) {
		this.core = core;
	}

	boolean start(String oname, Phiola.RecordParams p) {
		Calendar cal = new GregorianCalendar();
		cal.setTime(new Date());
		out = String.format("%s/rec_%04d%02d%02d_%02d%02d%02d.%s"
			, core.rec.rec_path
			, cal.get(Calendar.YEAR)
			, cal.get(Calendar.MONTH) + 1
			, cal.get(Calendar.DAY_OF_MONTH)
			, cal.get(Calendar.HOUR_OF_DAY)
			, cal.get(Calendar.MINUTE)
			, cal.get(Calendar.SECOND)
			, core.rec.rec_fmt);

		mr = new MediaRecorder();
		try {
			mr.setOnErrorListener((mr, what, extra) -> { on_error(); });
			mr.setAudioSource(MediaRecorder.AudioSource.MIC);
			mr.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4);
			mr.setAudioEncoder(MediaRecorder.AudioEncoder.AAC);
			mr.setAudioEncodingBitRate(p.quality * 1000);
			mr.setOutputFile(out);
			mr.prepare();
			mr.start();
		} catch (Exception e) {
			core.errlog(TAG, "MediaRecorder.prepare(): %s", e);
			mr.release();
			mr = null;
			return false;
		}
		return true;
	}

	private void on_error() {
		core.dbglog(TAG, "MediaRecorder error");
		core.tq.post(() -> core.rec_finished(0, 1, out));
	}

	String stop() {
		if (mr == null)
			return out;

		try {
			mr.stop();
		} catch (Exception e) {
			core.errlog(TAG, "MediaRecorder.stop(): %s", e);
		}
		mr.reset();
		mr.release();
		mr = null;
		return out;
	}
}
