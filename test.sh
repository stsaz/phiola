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

	./phiola co co.wav -f -o co-wav-s1-u2.wav -s 1 -u 2 ; ./phiola pl co-wav-s1-u2.wav
	# ./phiola co co.wav -f -o co-wav-96k.wav -rate 96000 ; ./phiola pl co-wav-96k.wav
	./phiola co co.wav -f -o co-wav-i24.wav -af int24 ; ./phiola pl co-wav-i24.wav
	./phiola co co.wav -f -o co-wav-mono.wav -ch 1 ; ./phiola pl co-wav-mono.wav
	./phiola co co.wav -f -o co-wav-gain6.wav -gain -6 ; ./phiola pl co-wav-gain6.wav
	./phiola co co.wav -f -o co-wav.wav -preserve-date

	convert_from_to wav m4a
	convert_from_to wav ogg
	# convert_from_to wav opus
	convert_from_to wav flac
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
	./phiola i pl.wav
	./phiola i pl.wav -tags

	./phiola i pl.wav -peaks
	./phiola i pl.wav -peaks -peaks-crc

	./phiola i fm-* -peaks
}

test_dir_read() {
	./phiola i -inc '*.wav' .
	./phiola i -inc '*.wav' -exc 'co*.wav' .
}

test_list() {
	if ! test -f list3.wav ; then
		./phiola rec -o list1.wav -f -u 2
		cp list1.wav list2.wav
		cp list1.wav list3.wav
	fi
	echo "!!! PRESS Shift+L !!!"
	./phiola `pwd`/list*.wav
	./phiola i /tmp/phiola-*.m3u8
}

test_clean() {
	rm -f *.wav co-* fm-* copy-*
}

TESTS=(
	device
	record
	play
	convert
	copy
	info
	dir_read
	list
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
