#!/bin/bash
# phiola test script

set -xe

if [[ $OS == Windows_NT ]]; then
	OS=windows
else
	OS=$(uname)
	if [[ $OS == Linux ]]; then
		OS=linux
	fi
fi
HAVE_FFMPEG=0
if command -v ffmpeg &>/dev/null; then
	HAVE_FFMPEG=1
fi

PHI_TEST_DIR="$(dirname "$0")"
source $PHI_TEST_DIR/test_convert.sh
source $PHI_TEST_DIR/test_metadata.sh
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
	./phiola server  -h
	./phiola tag     -h
	./phiola version -h
}

test_device() {
	./phiola dev
	./phiola dev list
	./phiola dev list -play
	./phiola dev list -cap
	./phiola dev list -filter test || true
	./phiola dev list -au unknown || true

	if [[ $OS == linux ]]; then
		./phiola dev list -filter Monitor -num
		./phiola dev list -au alsa
	fi
}

test_record() {
	./phiola rec || true
	./phiola -D rec -f -o rec.wav -u 1
	./phiola rec -f -o rec.wav -u 1 -dev 999 || true
	./phiola rec -f -o rec.wav -u 1 -dev 1
	./phiola -D rec -f -o rec.wav -u 1 -buf 1000 | grep -E 'opened audio capture buffer.*1000ms'
	./phiola -D rec -f -o rec.wav -u 1 -gain -6 | grep 'gain: -6dB'
	rm rec.wav ; ./phiola rec -o @stdout.wav -u 1 >rec.wav ; test -f rec.wav

	if [[ $OS == linux ]]; then
		./phiola -D rec -f -o rec.wav -u 1 -af int32 -rate 96000 -ch 1 | grep 'opened audio capture buffer: int32/96000/1'
	fi
}

test_record_split() {
	./phiola rec  -rate 48000  -split 0.250  -u 1  -f -o rec_split_@counter.flac
	./phiola info ./rec_split_1.flac | grep '12,000 samples'
	./phiola info ./rec_split_2.flac | grep '12,000 samples'
	./phiola info ./rec_split_3.flac | grep '12,000 samples'
	./phiola info ./rec_split_4.flac | grep '12,000 samples'
}

test_play() {
	./phiola pl || true

	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 1
	fi

	./phiola -D pl pl.wav
	./phiola pl pl.wav -dev 1
	./phiola -D pl pl.wav -buf 1000 | grep 'opened buffer 1000ms'

	if [[ $HAVE_FFMPEG -eq 1 ]]; then
		ffmpeg -i pl.wav -y -c:a pcm_u8 fm_pcm8.wav 2>/dev/null
		./phiola pl fm_pcm8.wav
	fi

	# seek/until
	./phiola pl pl.wav -s 0.500
	./phiola pl pl.wav -u 0.500
	./phiola pl pl.wav -s 0.250 -u 0.750

	# STDIN
	./phiola pl @stdin <pl.wav
	echo pl.wav | ./phiola pl @names

	./phiola pl pl.wav -perf
}

test_alsa() {
	./phiola rec -o rec.wav -f -u 1 -au alsa
	./phiola rec -o rec.wav -f -u 1 -au alsa -dev 1
	./phiola pl rec.wav -au alsa
	./phiola pl rec.wav -au alsa -dev 1
}

test_wasapi_exclusive() {
	./phiola rec -o rec.wav -f -u 1 -exclusive -buf 50
	./phiola pl rec.wav -exclusive -buf 50
}

test_wasapi_loopback() {
	./phiola rec -o lb.wav -f -u 5
	./phiola pl lb.wav &
	./phiola rec -o lb-rec.wav -f -u 4 -loopback
	kill $!
}

test_info() {
	if [[ ! -f 48_2.wav ]]; then
		./phiola rec -rate 48000 -o 48_2.wav -f -u 2
	fi

	./phiola i 48_2.wav
	./phiola i 48_2.wav -tags

	./phiola i 48_2.wav -peaks
	./phiola i 48_2.wav -loudness

	if [[ $HAVE_FFMPEG -eq 1 ]]; then
		if [[ ! -f fm_wv.wv ]]; then
			ffmpeg_encode 48_2.wav
		fi
		./phiola i fm_* -peaks
	fi
}

test_until() {
	if [[ ! -f 48_2.wav ]]; then
		./phiola rec -rate 48000 -o 48_2.wav -f -u 2
	fi
	if [[ ! -f fm_wv.wv ]]; then
		ffmpeg_encode 48_2.wav
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
	if [[ ! -f 48_2.wav ]]; then
		./phiola rec -rate 48000 -o 48_2.wav -f -u 2
	fi
	if [[ ! -f fm_wv.wv ]]; then
		ffmpeg_encode 48_2.wav
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
		./phiola rec -rate 48000 -o pl.wav -f -u 1
	fi

	# Chained OGG(Opus) stream
	./phiola co pl.wav -o ogg1.opus -f
	./phiola co pl.wav -o ogg2.opus -f
	cat ogg1.opus ogg2.opus >ogg3.opus
	./phiola i -peaks ogg3.opus
}

test_danorm() {
	if [[ ! -f dani.wav ]]; then
		./phiola rec -u 10 -f -o dani.wav
	fi

	./phiola co -danorm "frame 500 size 15" dani.wav -f -o dan_co.wav
	./phiola i -peaks dan_co.wav

	./phiola co -danorm "" dani.wav -f -o dan_co.flac -af int24
	./phiola i dan_co.flac | grep 'int24'
	./phiola i -peaks dan_co.flac

	# ./phiola co -danorm "" dani.wav -f -o dan_co96k.flac -af int24 -rate 96000
	# ./phiola dan_co96k.flac

	./phiola rec -danorm "frame 500 size 15" -f -o dan_rec.wav -u 10
	./phiola i -peaks dan_rec.wav

	if [[ $OS == linux ]]; then
		./phiola rec -danorm "" -f -o dan_rec96k.flac -u 10 -af int24 -rate 96000
		./phiola i dan_rec96k.flac | grep 'int24 96000Hz'
		./phiola dan_rec96k.flac
	fi
}

test_norm() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 1
	fi
	./phiola pl pl.wav -norm ""
}

test_equalizer() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 1
	fi
	./phiola pl -equ " f 1000 w 1.0q" pl.wav || true # missing parameter
	./phiola pl -equ " f 1000 unknown 1.0q g -6.0" pl.wav || true # unknown parameter
	./phiola -D pl -equ " w 1.0q g -6.0 f 1000 , w 1.0q f 10000 g -6.0 " pl.wav | grep 'adding filter'
	./phiola -D pl -equ "t bass g 6, f 1000 w 1.0q g 3, t treble g -6" pl.wav | grep 'adding filter'
}

test_dir_read() {
	if [[ ! -f pl.wav ]]; then
		./phiola rec -rate 48000 -o pl.wav -f -u 1
	fi
	mkdir -p phi_test/dir
	for i in 1.wav 2.wav skip1.wav skip2.wav; do
		cp pl.wav phi_test/dir/$i
	done
	[[ $(./phiola i -inc '*.wav' phi_test | grep -c .wav) == 4 ]]
	[[ $(./phiola i -inc '*.wav' -exc '*/skip*.wav' phi_test | grep -c .wav) == 2 ]]
	rm -rf phi_test
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

test_remote() {
	CHILD=$(./phiola -Background rec -f -o rec_remote.flac -remote)
	sleep 5
	./phiola remote stop
	sleep 1
	ps -q $CHILD && exit 1 # subprocess must exit
	./phiola remote stop || true
	./phiola i rec_remote.flac
}

test_clean() {
	rm -f *.wav *.flac *.m4a *.aac *.ogg *.opus *.mp3 fm_* *.cue *.m3u
	rm -rf ofv copa
}

TESTS=(
	device

	record
	record_split
	play

	info
	dir_read

	# convert_samples
	convert
	convert_encode
	convert_parallel
	ogg

	# Filtering
	danorm
	norm
	equalizer

	# Metadata
	meta_inject
	tag
	rename

	# Playlists & Files
	cue
	output_vars
	list
	list_heal

	# Streaming
	server
	# http
	tee

	remote
	help
)

if [[ $HAVE_FFMPEG -eq 1 ]]; then
	TESTS+=(
		until
		seek
		copy
	)
fi

if [[ $OS == linux ]]; then
	TESTS+=(
		alsa
	)
elif [[ $OS == windows ]]; then
	TESTS+=(
		wasapi_exclusive
		wasapi_loopback
	)
fi
TESTS+=(clean)

# Enable ALAC
echo DeprecatedMods >> phiola.conf

if [[ "$#" -gt 0 ]]; then
	TESTS=("$@")
fi

for T in "${TESTS[@]}" ; do
	test_$T
done

echo DONE
