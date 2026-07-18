#!/bin/bash

# phiola test script

set -xe

PHI_TEST_DIR="$(dirname "$0")"
source $PHI_TEST_DIR/test_convert.sh
source $PHI_TEST_DIR/test_list.sh
source $PHI_TEST_DIR/test_network.sh

test_help() {
	./phiola -h
	./phiola convert -h
	./phiola device  -h
	./phiola gui     -h
	./phiola info    -h
	./phiola list    -h
	./phiola play    -h
	./phiola record  -h
	./phiola remote  -h
	./phiola tag     -h
}

test_device() {
	./phiola dev
	./phiola dev list
	./phiola dev list -play
	./phiola dev list -cap
	./phiola dev list -filter Monitor -num
	./phiola dev list -filter test || true
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
}

test_record_split() {
	./phiola rec  -rate 48000  -split 1  -until 3  -f -o rec_split_@counter.flac
	./phiola info ./rec_split_1.flac | grep '48,000 samples'
	./phiola info ./rec_split_2.flac | grep '48,000 samples'
	./phiola info ./rec_split_3.flac | grep '48,000 samples'
}

test_record_manual() {
	echo "!!! PRESS CTRL+C MANUALLY !!!"
	./phiola rec -o rec.wav -f
}

test_play() {
	./phiola pl || true

	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi

	./phiola pl pl.wav
	./phiola pl pl.wav -dev 1
	./phiola -D pl pl.wav -buf 1000 | grep 'opened buffer 1000ms'

	ffmpeg -i pl.wav -y -c:a pcm_u8 fm_pcm8.wav 2>/dev/null
	./phiola pl fm_pcm8.wav

	# seek/until
	./phiola pl pl.wav -s 1
	./phiola pl pl.wav -u 1
	./phiola pl pl.wav -s 0.500 -u 1.500

	# STDIN
	./phiola pl @stdin <pl.wav
	echo pl.wav | ./phiola pl @names

	./phiola pl pl.wav -perf
}

test_rec_play_alsa() {
	./phiola rec -o rec.wav -f -u 2 -au alsa
	./phiola rec -o rec.wav -f -u 2 -au alsa -dev 1
	./phiola pl rec.wav -au alsa
	./phiola pl rec.wav -au alsa -dev 1
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

test_info() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi

	./phiola i pl.wav
	./phiola i pl.wav -tags

	./phiola i pl.wav -peaks
	./phiola i pl.wav -loudness

	if [[ ! -f fm_wv.wv ]]; then
		ffmpeg_encode pl.wav
	fi
	./phiola i fm_* -peaks
}

test_until() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi
	if [[ ! -f fm_wv.wv ]]; then
		ffmpeg_encode pl.wav
	fi

	./phiola i -peaks fm_aac.aac     | grep -E '9[67],... total'
	./phiola i -peaks fm_aac.avi     | grep -E '9[67],... total'
	./phiola i -peaks fm_aac.mkv     | grep -E '9[67],... total'
	./phiola i -peaks fm_aac.mp4     | grep -E '9[67],... total'
	./phiola i -peaks fm_alac.mkv    | grep -E '9[56],... total'
	./phiola i -peaks fm_alac.mp4    | grep '96,000 total'
	./phiola i -peaks fm_flac.flac   | grep '96,000 total'
	./phiola i -peaks fm_flac.ogg    | grep -E '96,... total'
	./phiola i -peaks fm_mp3.avi     | grep -E '9[67],... total'
	./phiola i -peaks fm_mp3.mkv     | grep -E '9[67],... total'
	./phiola i -peaks fm_mp3.mp3     | grep '96,000 total'
	./phiola i -peaks fm_mp3_320.mp3 | grep '96,000 total'
	./phiola i -peaks fm_opus.mkv    | grep -E '96,... total'
	./phiola i -peaks fm_opus.ogg    | grep '96,000 total'
	./phiola i -peaks fm_pcm.avi     | grep '96,000 total'
	./phiola i -peaks fm_pcm.caf     | grep '96,000 total'
	./phiola i -peaks fm_pcm.mkv     | grep '96,000 total'
	./phiola i -peaks fm_pcm.wav     | grep '96,000 total'
	./phiola i -peaks fm_vorbis.mkv  | grep -E '9[56],... total'
	./phiola i -peaks fm_vorbis.ogg  | grep '96,000 total'
	./phiola i -peaks fm_wv.wv       | grep '96,000 total'

	## the ffmpeg-generated mkv file may contain the blocks with +1ms greater start position
	./phiola i -peaks -u 1 fm_aac.aac     | grep '48,000 total'
	# ./phiola i -peaks -u 1 fm_aac.avi     | grep -E '4[89],... total'
	./phiola i -peaks -u 1 fm_aac.mkv     | grep -E '48,... total'
	./phiola i -peaks -u 1 fm_aac.mp4     | grep '48,000 total'
	./phiola i -peaks -u 1 fm_alac.mkv    | grep -E '4[78],... total'
	./phiola i -peaks -u 1 fm_alac.mp4    | grep '48,000 total'
	./phiola i -peaks -u 1 fm_flac.flac   | grep '48,000 total'
	./phiola i -peaks -u 1 fm_flac.ogg    | grep '48,000 total'
	./phiola i -peaks -u 1 fm_mp3.avi     | grep '48,000 total'
	./phiola i -peaks -u 1 fm_mp3.mkv     | grep '48,000 total'
	./phiola i -peaks -u 1 fm_mp3.mp3     | grep '48,000 total'
	./phiola i -peaks -u 1 fm_mp3_320.mp3 | grep '48,000 total'
	./phiola i -peaks -u 1 fm_opus.mkv    | grep -E '4[78],... total'
	./phiola i -peaks -u 1 fm_opus.ogg    | grep '48,000 total'
	./phiola i -peaks -u 1 fm_pcm.avi     | grep '48,000 total'
	./phiola i -peaks -u 1 fm_pcm.caf     | grep '48,000 total'
	./phiola i -peaks -u 1 fm_pcm.mkv     | grep -E '4[78],... total'
	./phiola i -peaks -u 1 fm_pcm.wav     | grep '48,000 total'
	./phiola i -peaks -u 1 fm_vorbis.mkv  | grep -E '4[78],... total'
	./phiola i -peaks -u 1 fm_vorbis.ogg  | grep '48,000 total'
	./phiola i -peaks -u 1 fm_wv.wv       | grep '48,000 total'
}

test_seek() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi
	if [[ ! -f fm_wv.wv ]]; then
		ffmpeg_encode pl.wav
	fi

	## mkv seeking implementation is not precise
	## 128k mp3 has smaller frame size than 320k
	## ogg: first packet is skipped if the target page has 'continued' flag
	./phiola i -peaks -s 1 fm_aac.aac     | grep -E '4[89],... total'
	# ./phiola i -peaks -s 1 fm_aac.avi     | grep -E '4[89],... total'
	./phiola i -peaks -s 1 fm_aac.mkv     | grep -E '4[89],... total'
	./phiola i -peaks -s 1 fm_aac.mp4     | grep -E '4[89],... total'
	./phiola i -peaks -s 1 fm_alac.mkv    | grep -E '4[678],... total'
	./phiola i -peaks -s 1 fm_alac.mp4    | grep '48,000 total'
	./phiola i -peaks -s 1 fm_flac.flac   | grep '48,000 total'
	./phiola i -peaks -s 1 fm_flac.ogg    | grep -E '4[45678],... total'
	# ./phiola i -peaks -s 1 fm_mp3.avi     | grep -E '4[89],... total'
	./phiola i -peaks -s 1 fm_mp3.mkv     | grep -E '4[89],... total'
	./phiola i -peaks -s 1 fm_mp3.mp3     | grep -E '50,... total'
	./phiola i -peaks -s 1 fm_mp3_320.mp3 | grep -E '49,... total'
	./phiola i -peaks -s 1 fm_opus.mkv    | grep -E '4[78],... total'
	./phiola i -peaks -s 1 fm_opus.ogg    | grep '48,000 total'
	# ./phiola i -peaks -s 1 fm_pcm.avi     | grep -E '4[89],... total'
	# ./phiola i -peaks -s 1 fm_pcm.caf     | grep -E '4[89],... total'
	./phiola i -peaks -s 1 fm_pcm.mkv     | grep -E '4[678],... total'
	./phiola i -peaks -s 1 fm_pcm.wav     | grep '48,000 total'
	./phiola i -peaks -s 1 fm_vorbis.mkv  | grep -E '4[678],... total'
	./phiola i -peaks -s 1 fm_vorbis.ogg  | grep -E '4[78],... total'
	./phiola i -peaks -s 1 fm_wv.wv       | grep '48,000 total'
}

test_ogg() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi

	# Chained OGG(Opus) stream
	./phiola co pl.wav -o ogg1.opus -f
	./phiola co pl.wav -o ogg2.opus -f
	cat ogg1.opus ogg2.opus >ogg3.opus
	./phiola pl ogg3.opus
}

test_danorm() {
	if [[ ! -f dani.wav ]]; then
		./phiola rec -u 10 -f -o dani.wav
	fi

	./phiola co -danorm "frame 500 size 15" dani.wav -f -o dan_co.wav
	./phiola dan_co.wav

	./phiola co -danorm "" dani.wav -f -o dan_co.flac -af int24
	./phiola i dan_co.flac | grep 'int24'
	./phiola dan_co.flac

	# ./phiola co -danorm "" dani.wav -f -o dan_co96k.flac -af int24 -rate 96000
	# ./phiola dan_co96k.flac

	./phiola rec -danorm "frame 500 size 15" -f -o dan_rec.wav -u 10
	./phiola dan_rec.wav

	./phiola rec -danorm "" -f -o dan_rec96k.flac -u 10 -af int24 -rate 96000
	./phiola i dan_rec96k.flac | grep 'int24 96000Hz'
	./phiola dan_rec96k.flac
}

test_norm() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi
	./phiola pl pl.wav -norm ""
}

test_equalizer() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi
	./phiola pl -equ " f 1000 w 1.0q" pl.wav || true # missing parameter
	./phiola pl -equ " f 1000 unknown 1.0q g -6.0" pl.wav || true # unknown parameter
	./phiola -D pl -equ " w 1.0q g -6.0 f 1000 , w 1.0q f 10000 g -6.0 " pl.wav | grep 'adding filter'
	./phiola -D pl -equ "t bass g 6, f 1000 w 1.0q g 3, t treble g -6" pl.wav | grep 'adding filter'
}

test_dir_read() {
	./phiola i -inc '*.wav' .
	./phiola i -inc '*.wav' -exc 'co*.wav' .
}

test_tee() {
	if [[ ! -f tee.opus ]]; then
		./phiola rec -o tee.opus -f -u 2
	fi

	rm teeout.opus || true
	cat tee.opus | ./phiola pl @stdin -tee teeout.opus
	./phiola pl teeout.opus
	rm teeout.opus
}

test_ofile_vars() {
	if [[ ! -f ofv.wav ]]; then
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
	CHILD=$(./phiola -Background rec -f -o rec_remote.flac -remote)
	sleep 5
	./phiola remote stop
	sleep 1
	ps -q $CHILD && exit 1 # subprocess must exit
	./phiola remote stop || true
	./phiola i rec_remote.flac
}

test_tag() {
	if [[ ! -f tag.mp3 ]]; then
		./phiola rec -o tag.wav -f -u 2
		./phiola co tag.wav -o .mp3
		./phiola co tag.wav -o .ogg
		./phiola co tag.wav -o .opus
		./phiola co tag.wav -o .flac
	fi

	# add new tags
	./phiola tag -m 'artist=Great Artist' -m 'title=Cool Song' -m 'usertag=User Data' tag.mp3 tag.ogg tag.opus tag.flac
	./phiola i tag.mp3 | grep "Great Artist - Cool Song"
	./phiola i tag.ogg | grep "Great Artist - Cool Song"
	./phiola i tag.opus | grep "Great Artist - Cool Song"
	./phiola i tag.flac | grep "Great Artist - Cool Song"
	./phiola i -tag tag.mp3 | grep -iE "usertag.*User Data"
	./phiola i -tag tag.ogg | grep -iE "usertag.*User Data"
	./phiola i -tag tag.opus | grep -iE "usertag.*User Data"
	./phiola i -tag tag.flac | grep -iE "usertag.*User Data"

	# replace tag
	./phiola tag -m 'title=Very Cool Song' tag.mp3 tag.ogg tag.opus tag.flac
	./phiola i tag.mp3 | grep "Great Artist - Very Cool Song"
	./phiola i tag.ogg | grep "Great Artist - Very Cool Song"
	./phiola i tag.opus | grep "Great Artist - Very Cool Song"
	./phiola i tag.flac | grep "Great Artist - Very Cool Song"
	./phiola i -tag tag.mp3 | grep -iE "usertag.*User Data"
	./phiola i -tag tag.ogg | grep -iE "usertag.*User Data"
	./phiola i -tag tag.opus | grep -iE "usertag.*User Data"
	./phiola i -tag tag.flac | grep -iE "usertag.*User Data"

	# set tag
	./phiola tag -clear -m 'title=Cool Song' tag.mp3 tag.ogg tag.opus tag.flac
	./phiola i tag.mp3 | grep " - Cool Song"
	./phiola i tag.ogg | grep " - Cool Song"
	./phiola i tag.opus | grep " - Cool Song"
	./phiola i tag.flac | grep " - Cool Song"

	# error
	./phiola tag -m 'badtag' tag.mp3 || true
}

test_rename() {
	if [[ ! -f rename2.opus ]]; then
		./phiola rec -o rename.wav -f -u 2
		./phiola co rename.wav -o rename1.opus -m "artist=A1" -m "title=T1"
		./phiola co rename.wav -o rename2.opus -m "artist=A2" -m "title=T2"
	fi

	./phiola rename rename1.opus rename2.opus -o "renamed @artist - @title"
	./phiola i \
		"renamed A1 - T1.opus" \
		"renamed A2 - T2.opus"
}

test_clean() {
	rm -f *.wav *.flac *.m4a *.aac *.ogg *.opus *.mp3 fm_* ofv/*.ogg *.cue *.m3u copa/*
	rmdir ofv copa
}

TESTS=(
	device
	record
	record_split
	# record_manual
	play
	# convert_samples
	convert
	convert_encode
	convert_parallel
	info
	until
	seek
	ogg
	copy
	meta
	danorm
	norm
	dir_read
	list
	list_heal
	# list_manual
	cue
	ofile_vars
	remote
	tag
	rename
	server
	# http
	tee
	equalizer
	clean
	# rec_play_alsa
	# wasapi_exclusive
	# wasapi_loopback
	help
	)

if [[ "$#" -gt 0 ]]; then
	TESTS=("$@")
fi

for T in "${TESTS[@]}" ; do
	test_$T
done

echo DONE
