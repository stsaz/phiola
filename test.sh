#!/bin/bash

# phiola test script

set -xe

test_device() {
	./phiola dev
	./phiola dev list
	./phiola dev list -au alsa
	./phiola dev list -au unknown || true
}

test_record() {
	./phiola rec || true

	./phiola rec -f -o rec.wav -u 2

	./phiola rec -f -o rec.wav -u 2 -dev 999 || true
	./phiola rec -f -o rec.wav -u 2 -dev 1

	./phiola -D rec -f -o rec.wav -u 2 -buf 1000 | grep -E 'opened audio capture buffer.*1000ms'
	./phiola -D rec -f -o rec.wav -u 2 -af int32 -rate 96000 -ch 1 | grep 'opened audio capture buffer: int32/96000/1'

	# filters
	./phiola -D rec -f -o rec.wav -u 2 -gain -6 | grep 'gain: -6dB'

	# STDOUT
	rm rec.wav ; ./phiola rec -o @stdout.wav -u 2 >rec.wav ; test -f rec.wav

	# ALSA
	sleep 5 # let PulseAudio unlock devices
	./phiola rec -o rec.wav -f -u 2 -au alsa
	./phiola rec -o rec.wav -f -u 2 -au alsa -dev 1

	echo "!!! PRESS CTRL+C MANUALLY !!!"
	./phiola rec -o rec.wav -f
}

test_play() {
	./phiola pl || true

	if ! test -f pl.wav ; then
		./phiola rec -o pl.wav -f -u 2
	fi

	./phiola pl pl.wav
	./phiola pl pl.wav -dev 1
	./phiola -D pl pl.wav -buf 1000 | grep 'opened buffer 1000ms'

	# seek/until
	./phiola pl pl.wav -s 1
	./phiola pl pl.wav -u 1
	./phiola pl pl.wav -s 0.500 -u 1.500

	# STDIN
	./phiola pl @stdin <pl.wav

	./phiola pl pl.wav -perf

	# ALSA
	sleep 10 # let PulseAudio unlock devices
	./phiola pl pl.wav -au alsa
	./phiola pl pl.wav -au alsa -dev 1
}

test_wasapi_exclusive() {
	./phiola rec -o rec.wav -f -u 5 -exclusive -buf 50
	./phiola pl rec.wav -exclusive -buf 50
}

test_wasapi_loopback() {
	./phiola rec -o lb.wav -f -u 5
	./phiola pl lb.wav &
	./phiola rec -o lb-rec.wav -f -u 4 -loopback
	kill $!
}

convert_from_to() {
	./phiola co co.$1 -f -o co-$1.$2 ; ./phiola pl co-$1.$2
}

test_convert() {
	./phiola co || true

	if ! test -f co.wav ; then
		./phiola rec -o co.wav -f -u 2
	fi

	# std
	./phiola co @stdin -f -o co-std.wav <co.wav ; ./phiola pl co-std.wav
	./phiola co co.wav -f -o @stdout.wav >co-std.wav ; ./phiola pl co-std.wav
	./phiola co @stdin -f -o @stdout.wav <co.wav >co-std.wav ; ./phiola pl co-std.wav

	# seek/until
	./phiola co co.wav -f -o co-wav-s1-u2.wav -s 1 -u 2 ; ./phiola pl co-wav-s1-u2.wav

	# audio format
	./phiola co co.wav -f -o co-wav-i24.wav -af int24 ; ./phiola i co-wav-i24.wav | grep 'int24' ; ./phiola pl co-wav-i24.wav
	./phiola co co.wav -f -o co-wav-mono.wav -ch 1 ; ./phiola i co-wav-mono.wav | grep 'mono' ; ./phiola pl co-wav-mono.wav
	./phiola co co.wav -f -o co-wav-96k.wav -rate 96000 ; ./phiola i co-wav-96k.wav | grep '96000Hz' ; ./phiola pl co-wav-96k.wav
	./phiola co co.wav -f -o co-wav-i32-96k.wav -af int32 -rate 96000 ; ./phiola i co-wav-i32-96k.wav | grep 'int32 96000Hz' ; ./phiola pl co-wav-i32-96k.wav

	./phiola co co.wav -f -o co-wav-gain6.wav -gain -6 ; ./phiola pl co-wav-gain6.wav
	./phiola co co.wav -f -o co-wav.wav -preserve_date

	convert_from_to wav m4a
	convert_from_to wav ogg
	convert_from_to wav opus
	convert_from_to wav flac
}

test_danorm() {
	if ! test -f dani.wav ; then
		./phiola rec -u 10 -f -o dani.wav
	fi
	./phiola co -danorm "frame 500 size 15" dani.wav -f -o dan-co.wav ; ./phiola dan-co.wav
	./phiola co -danorm "" dani.wav -f -o dan-co.flac -af int24 ; ./phiola i dan-co.flac | grep 'int24' ; ./phiola dan-co.flac
	# ./phiola co -danorm "" dani.wav -f -o dan-co96k.flac -af int24 -rate 96000 ; ./phiola dan-co96k.flac
	./phiola rec -danorm "frame 500 size 15" -f -o dan-rec.wav     -u 10 ; ./phiola dan-rec.wav
	./phiola rec -danorm "" -f -o dan-rec96k.flac -u 10 -af int24 -rate 96000 ; ./phiola i dan-rec96k.flac | grep 'int24 96000Hz' ; ./phiola dan-rec96k.flac
}

ffmpeg_encode() {
	ffmpeg -i $1 -y -c:a aac        fm-aac.aac    2>/dev/null
	ffmpeg -i $1 -y -c:a aac        fm-aac.avi    2>/dev/null
	ffmpeg -i $1 -y -c:a aac        fm-aac.mkv    2>/dev/null
	ffmpeg -i $1 -y -c:a aac        fm-aac.mp4    2>/dev/null
	ffmpeg -i $1 -y -c:a alac       fm-alac.mkv   2>/dev/null
	ffmpeg -i $1 -y -c:a alac       fm-alac.mp4   2>/dev/null
	ffmpeg -i $1 -y -c:a flac       fm-flac.flac  2>/dev/null
	ffmpeg -i $1 -y -c:a flac       fm-flac.ogg   2>/dev/null
	ffmpeg -i $1 -y -c:a libmp3lame fm-mp3.avi    2>/dev/null
	ffmpeg -i $1 -y -c:a libmp3lame fm-mp3.mkv    2>/dev/null
	ffmpeg -i $1 -y -c:a libmp3lame fm-mp3.mp3    2>/dev/null
	ffmpeg -i $1 -y -c:a libopus    fm-opus.mkv   2>/dev/null
	ffmpeg -i $1 -y -c:a libopus    fm-opus.ogg   2>/dev/null
	ffmpeg -i $1 -y -c:a libvorbis  fm-vorbis.mkv 2>/dev/null
	ffmpeg -i $1 -y -c:a libvorbis  fm-vorbis.ogg 2>/dev/null
	ffmpeg -i $1 -y -c:a pcm_s16le  fm-pcm.avi    2>/dev/null
	ffmpeg -i $1 -y -c:a pcm_s16le  fm-pcm.caf    2>/dev/null
	ffmpeg -i $1 -y -c:a pcm_s16le  fm-pcm.mkv    2>/dev/null
	ffmpeg -i $1 -y -c:a pcm_s16le  fm-pcm.wav    2>/dev/null
	ffmpeg -i $1 -y -c:a wavpack    fm-wv.wv      2>/dev/null
}

test_copy() {
	if ! test -f co.wav ; then
		./phiola rec -o co.wav -f -u 2
	fi

	if ! test -f fm-wv.wv ; then
		ffmpeg_encode co.wav
	fi

	./phiola co -copy -f -s 1 -u 2 co.wav -o copy-wav.wav ; ./phiola pl copy-wav.wav
	./phiola co -copy -f -s 1 -u 2 fm-aac.aac -o copy-aac.m4a ; ./phiola pl copy-aac.m4a
	# ./phiola co -copy -f -s 1 -u 2 fm-aac.mkv -o copy-aac-mkv.m4a ; ./phiola pl copy-aac-mkv.m4a
	./phiola co -copy -f -s 1 -u 2 fm-aac.mp4 -o copy-mp4.m4a ; ./phiola pl copy-mp4.m4a
	./phiola co -copy -f -s 1 -u 2 fm-mp3.mkv -o copy-mp3-mkv.mp3 ; ./phiola pl copy-mp3-mkv.mp3
	./phiola co -copy -f -s 1 -u 2 fm-mp3.mp3 -o copy-mp3.mp3 ; ./phiola pl copy-mp3.mp3
	./phiola co -copy -f -s 1 -u 2 fm-opus.mkv -o copy-opus-mkv.ogg ; ./phiola pl copy-opus-mkv.ogg
	./phiola co -copy -f -s 1 -u 2 fm-opus.ogg -o copy-opus.ogg ; ./phiola pl copy-opus.ogg
	# ./phiola co -copy -f -s 1 -u 2 fm-vorbis.mkv -o copy-vorbis-mkv.ogg ; ./phiola pl copy-vorbis-mkv.ogg
	./phiola co -copy -f -s 1.50 -u 2 fm-vorbis.ogg -o copy-vorbis.ogg ; ./phiola pl copy-vorbis.ogg
}

test_info() {
	if ! test -f pl.wav ; then
		./phiola rec -o pl.wav -f -u 2
	fi
	./phiola i pl.wav
	./phiola i pl.wav -tags

	./phiola i pl.wav -peaks
	./phiola i pl.wav -peaks -peaks_crc

	if ! test -f fm-wv.wv ; then
		ffmpeg_encode co.wav
	fi
	./phiola i fm-* -peaks
}

test_dir_read() {
	./phiola i -inc '*.wav' .
	./phiola i -inc '*.wav' -exc 'co*.wav' .
}

test_list() {
	if ! test -f list3.ogg ; then
		./phiola rec -o list1.wav -f -u 2
		./phiola co list1.wav -m artist=A2 -m title=T2 -f -o list2.ogg
		./phiola co list1.wav -m artist=A3 -m title=T3 -f -o list3.ogg
	fi

	./phiola list create . -include "list*.ogg" -o test.m3u
	./phiola info test.m3u | grep '#1 "A2 - T2" "./list2.ogg"'

	./phiola list create list3.ogg list2.ogg -o test-sort.m3u
	./phiola list create list3.ogg list2.ogg -o test-sort2.m3u
	./phiola list sort test-sort.m3u test-sort2.m3u
	./phiola info test-sort.m3u | grep '#1 "A2 - T2" "list2.ogg"'
	./phiola info test-sort2.m3u | grep '#1 "A2 - T2" "list2.ogg"'

	cat <<EOF >/tmp/phiola-test.pls
[playlist]
File1=`pwd`/list1.wav
Title1=TITLE1
Length1=1
File2=`pwd`/list2.ogg
Title2=TITLE2
Length2=2
EOF
	./phiola i /tmp/phiola-test.pls
}

test_list_manual() {
	echo "!!! PRESS Shift+L at the 3rd track !!!"
	./phiola `pwd`/list*
	cat /tmp/phiola-*.m3u8 | grep 'A2 - T2'
	# cat /tmp/phiola-*.m3u8 | grep 'A3 - T3'
	./phiola i /tmp/phiola-*.m3u8 | grep 'A2 - T2'

	LIST=`ls -1 /tmp/phiola-*.m3u8 | head -1`
	zstd $LIST -o $LIST.m3uz
	./phiola i $LIST.m3uz
}

test_cue() {
	if ! test -f "rec6.wav" ; then
		./phiola rec -u 6 -o rec6.wav -f
	fi
	cat <<EOF >cue.cue
PERFORMER Artist
FILE "rec6.wav" WAVE
 TRACK 01 AUDIO
  PERFORMER A1
  TITLE T1
  INDEX 01 00:00:00
 TRACK 02 AUDIO
  TITLE T2
  INDEX 01 00:02:00
 TRACK 03 AUDIO
  TITLE T3
  INDEX 01 00:04:00
EOF
	./phiola i cue.cue | grep 'A1 - T1'
	./phiola i cue.cue | grep 'Artist - T2'
	./phiola i cue.cue | grep 'Artist - T3'
	./phiola cue.cue
	if ./phiola i cue.cue -tracks 2,3 | grep 'A1 - T1' ; then
		false
	fi
	./phiola i cue.cue -tracks 2,3 | grep 'Artist - T2'
	./phiola i cue.cue -tracks 2,3 | grep 'Artist - T3'
}

test_meta() {
	# Recording
	./phiola rec -u 1 -m artist='Great Artist' -m title='Cool Song' -f -o meta.flac && ./phiola i meta.flac | grep 'Great Artist - Cool Song' || false
	./phiola rec -u 1 -m artist='Great Artist' -m title='Cool Song' -f -o meta.m4a && ./phiola i meta.m4a | grep 'Great Artist - Cool Song' || false
	./phiola rec -u 1 -m artist='Great Artist' -m title='Cool Song' -f -o meta.ogg && ./phiola i meta.ogg | grep 'Great Artist - Cool Song' || false
	./phiola rec -u 1 -m artist='Great Artist' -m title='Cool Song' -ra 48000 -f -o meta.opus && ./phiola i meta.opus | grep 'Great Artist - Cool Song' || false

	# Conversion
	./phiola co -m artist='AA' meta.flac -f -o meta2.flac && ./phiola i meta2.flac | grep 'AA - Cool Song' || false
	./phiola co -copy -m artist='AA' meta.m4a -f -o meta2.m4a && ./phiola i meta2.m4a | grep 'AA - Cool Song' || false
	./phiola co -m artist='AA' meta.ogg -f -o meta2.ogg && ./phiola i meta2.ogg | grep 'AA - Cool Song' || false
	ffmpeg -y -i meta.ogg -metadata artist='Great Artist' -metadata title='Cool Song' meta.mp3
	./phiola co -copy -m artist='AA' meta.mp3 -f -o meta2.mp3 && ./phiola i meta2.mp3 | grep 'AA - Cool Song' || false
}

test_http() {
	./phiola pl "http://localhost:1/" || true # no connection
	netmill http l 8080 w . &
	sleep .5
	cp -au $HTTP_DIR/$HTTP_FILE ./$HTTP_FILE
	echo "http://localhost:8080/$HTTP_FILE" >./http.m3u
	./phiola pl "http://localhost:8080/404" || true # http error
	./phiola pl "http://localhost:8080/$HTTP_FILE"
	./phiola pl "http://localhost:8080/http.m3u"
	kill $!
}

test_ofile_vars() {
	if ! test -f ofv.wav ; then
		./phiola rec -o ofv.wav -f -u 2
	fi
	./phiola co ofv.wav -f -o .ogg ; ./phiola i ofv.ogg
	mkdir -p ofv ; ./phiola co ofv.wav -f -o ofv/.ogg ; ./phiola i ofv/ofv.ogg
	./phiola co ofv.wav -f -o .ogg ; ./phiola i ofv.ogg
	./phiola co ofv.wav -f -o ofv/ofv.ogg -m 'artist=A' -m 'title=T' ; ./phiola i ofv/ofv.ogg
	./phiola co ofv/ofv.ogg -copy -f -o @filepath/@filename-@artist-@title.ogg ; ./phiola i ofv/ofv-A-T.ogg
	./phiola co ofv/ofv.ogg -copy -f -o ofv/@nowdate-@nowtime-@counter.ogg ; ./phiola i ofv/*-1.ogg
}

test_remote() {
	./phiola rec -f -o rec-remote.flac -remote &
	sleep 5
	./phiola remote stop
	wait $!
	./phiola i rec-remote.flac
}

test_clean() {
	rm -f *.wav *.flac *.m4a *.ogg *.opus *.mp3 fm-* ofv/*.ogg *.cue *.m3u
	rmdir ofv
}

TESTS=(
	device
	record
	play
	convert
	danorm
	copy
	info
	meta
	dir_read
	list
	# list_manual
	cue
	ofile_vars
	remote
	# http
	clean
	# wasapi_exclusive
	# wasapi_loopback
	)
if test "$#" -gt "0" ; then
	TESTS=("$@")
fi

for T in "${TESTS[@]}" ; do
	test_$T
done

echo DONE
