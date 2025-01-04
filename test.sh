#!/bin/bash

# phiola test script

set -xe

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

	if ! test -f pl.wav ; then
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
	if ! test -f pl.wav ; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi

	./phiola i pl.wav
	./phiola i pl.wav -tags

	./phiola i pl.wav -peaks
	./phiola i pl.wav -loudness

	if ! test -f fm_wv.wv ; then
		ffmpeg_encode pl.wav
	fi
	./phiola i fm_* -peaks
}

test_until() {
	if ! test -f pl.wav ; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi
	if ! test -f fm_wv.wv ; then
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
	./phiola i -peaks -u 1 fm_pcm.mkv     | grep -E '48,... total'
	./phiola i -peaks -u 1 fm_pcm.wav     | grep '48,000 total'
	./phiola i -peaks -u 1 fm_vorbis.mkv  | grep -E '4[78],... total'
	./phiola i -peaks -u 1 fm_vorbis.ogg  | grep '48,000 total'
	./phiola i -peaks -u 1 fm_wv.wv       | grep '48,000 total'
}

test_seek() {
	if ! test -f pl.wav ; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi
	if ! test -f fm_wv.wv ; then
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
	./phiola i -peaks -s 1 fm_pcm.mkv     | grep -E '4[78],... total'
	./phiola i -peaks -s 1 fm_pcm.wav     | grep '48,000 total'
	./phiola i -peaks -s 1 fm_vorbis.mkv  | grep -E '4[678],... total'
	./phiola i -peaks -s 1 fm_vorbis.ogg  | grep -E '4[78],... total'
	./phiola i -peaks -s 1 fm_wv.wv       | grep '48,000 total'
}

test_ogg() {
	if ! test -f pl.wav ; then
		./phiola rec -rate 48000 -o pl.wav -f -u 2
	fi

	# Chained OGG(Opus) stream
	./phiola co pl.wav -o ogg1.opus -f
	./phiola co pl.wav -o ogg2.opus -f
	cat ogg1.opus ogg2.opus >ogg3.opus
	./phiola pl ogg3.opus
}

test_convert_af() {
	O=co_wav_i24.wav          ; ./phiola co co.wav -af int24                   -f -o $O ; ./phiola i $O | grep 'int24' ; ./phiola pl $O
	O=co_wav_mono.wav         ; ./phiola co co.wav                       -ch 1 -f -o $O ; ./phiola i $O | grep 'mono' ; ./phiola pl $O
	O=co_wav_i24_mono.wav     ; ./phiola co co.wav -af int24             -ch 1 -f -o $O ; ./phiola i $O | grep 'int24 48000Hz mono' ; ./phiola pl $O
	O=co_wav_96k.wav          ; ./phiola co co.wav           -rate 96000       -f -o $O ; ./phiola i $O | grep '96000Hz' ; ./phiola pl $O
	O=co_wav_i32_96k.wav      ; ./phiola co co.wav -af int32 -rate 96000       -f -o $O ; ./phiola i $O | grep 'int32 96000Hz' ; ./phiola pl $O
	O=co_wav_i32_96k_mono.wav ; ./phiola co co.wav -af int32 -rate 96000 -ch 1 -f -o $O ; ./phiola i $O | grep 'int32 96000Hz mono' ; ./phiola pl $O
	# O=co_wav_i24_96k_mono.wav ; ./phiola co co.wav -af int24 -rate 96000 -ch 1 -f -o $O ; ./phiola i $O | grep 'int24 96000Hz mono' ; ./phiola pl $O
}

test_convert() {
	./phiola co || true

	if ! test -f co.wav ; then
		./phiola rec -rate 48000 -o co.wav -f -u 2
	fi

	# std
	./phiola co @stdin -f -o co_std.wav <co.wav ; ./phiola pl co_std.wav
	./phiola co co.wav -f -o @stdout.wav >co_std.wav ; ./phiola pl co_std.wav
	./phiola co @stdin -f -o @stdout.wav <co.wav >co_std.wav ; ./phiola pl co_std.wav

	# seek/until
	./phiola co co.wav -f -o co_wav_s1-u2.wav -s 1 -u 2 ; ./phiola pl co_wav_s1-u2.wav

	test_convert_af

	./phiola co co.wav -f -o co_wav_gain6.wav -gain -6 ; ./phiola pl co_wav_gain6.wav
	./phiola co co.wav -f -o co_wav.wav -preserve_date
}

convert__from_to() {
	./phiola co co.$1 -f -o co_$1.$2 ; ./phiola pl co_$1.$2
}

test_convert_encode() {
	if ! test -f co.wav ; then
		./phiola rec -rate 48000 -f -o co.wav -u 2
	fi

	convert__from_to wav flac
	./phiola i co_wav.flac             | grep '96,000 samples'
	./phiola i co_wav.flac -peaks      | grep '96,000 total'
	./phiola i -u 1 co_wav.flac -peaks | grep '48,000 total'
	# ./phiola i -s 1 co_wav.flac -peaks | grep '48,000 total'

	convert__from_to wav m4a
	./phiola i co_wav.m4a              | grep -E '98,... samples'
	./phiola i co_wav.m4a -peaks       | grep -E '96,... total'
	./phiola i -u 1 co_wav.m4a -peaks  | grep '48,000 total'
	./phiola i -s 1 co_wav.m4a -peaks  | grep -E '48,... total'

	./phiola co co.wav -aac_profile HE -f -o co_wav_he.m4a
	./phiola pl co_wav_he.m4a | grep 'HE-AAC'

	./phiola co co.wav -aac_profile HE2 -f -o co_wav_he2.m4a
	./phiola pl co_wav_he2.m4a | grep 'HE-AACv2'

	convert__from_to wav ogg
	./phiola i co_wav.ogg              | grep -E '95,... samples'
	./phiola i co_wav.ogg -peaks       | grep '96,000 total'
	./phiola i -u 1 co_wav.ogg -peaks  | grep -E '48,... total'
	./phiola i -s 1 co_wav.ogg -peaks  | grep -E '4[78],... total'

	convert__from_to wav opus
	./phiola i co_wav.opus             | grep -E '94,... samples'
	./phiola i co_wav.opus -peaks      | grep '96,000 total'
	./phiola i -u 1 co_wav.opus -peaks | grep '48,000 total'
	./phiola i -s 1 co_wav.opus -peaks | grep -E '46,... total'
}

test_convert_parallel() {
	if ! test -f co.wav ; then
		./phiola rec -u 2 -rate 48000 -o co.wav -f
	fi

	if ! test -f copa/co99.wav ; then
		mkdir -p copa
		for i in $(seq 1 99) ; do
			cp -u co.wav copa/co$i.wav
		done
	fi

	./phiola co copa -inc '*.wav' -u 1 -o copa/.flac -f
	./phiola i copa/*.flac
}

ffmpeg_encode() {
	ffmpeg -i $1 -y -c:a aac        fm_aac.aac    2>/dev/null
	ffmpeg -i $1 -y -c:a aac        fm_aac.avi    2>/dev/null
	ffmpeg -i $1 -y -c:a aac        fm_aac.mkv    2>/dev/null
	ffmpeg -i $1 -y -c:a aac        fm_aac.mp4    2>/dev/null
	ffmpeg -i $1 -y -c:a alac       fm_alac.mkv   2>/dev/null
	ffmpeg -i $1 -y -c:a alac       fm_alac.mp4   2>/dev/null
	ffmpeg -i $1 -y -c:a flac       fm_flac.flac  2>/dev/null
	ffmpeg -i $1 -y -c:a flac       fm_flac.ogg   2>/dev/null
	ffmpeg -i $1 -y -c:a libmp3lame fm_mp3.avi    2>/dev/null
	ffmpeg -i $1 -y -c:a libmp3lame fm_mp3.mkv    2>/dev/null
	ffmpeg -i $1 -y -c:a libmp3lame fm_mp3.mp3    2>/dev/null
	ffmpeg -i $1 -y -c:a libmp3lame -b:a 320k fm_mp3_320.mp3 2>/dev/null
	ffmpeg -i $1 -y -c:a libopus    fm_opus.mkv   2>/dev/null
	ffmpeg -i $1 -y -c:a libopus    fm_opus.ogg   2>/dev/null
	ffmpeg -i $1 -y -c:a libvorbis  fm_vorbis.mkv 2>/dev/null
	ffmpeg -i $1 -y -c:a libvorbis  fm_vorbis.ogg 2>/dev/null
	ffmpeg -i $1 -y -c:a pcm_s16le  fm_pcm.avi    2>/dev/null
	ffmpeg -i $1 -y -c:a pcm_s16le  fm_pcm.caf    2>/dev/null
	ffmpeg -i $1 -y -c:a pcm_s16le  fm_pcm.mkv    2>/dev/null
	ffmpeg -i $1 -y -c:a pcm_s16le  fm_pcm.wav    2>/dev/null
	ffmpeg -i $1 -y -c:a wavpack    fm_wv.wv      2>/dev/null
}

test_copy_until() {
	local INFO_N=$3
	if test "$#" == 4 ; then
		INFO_N=$4
	fi
	./phiola co -copy -f -u 1 $1 -o $2
	./phiola i $2 | grep -E "$INFO_N"
	./phiola i -peaks $2 | grep -E "$3"
}

test_copy_seek() {
	local INFO_N=$3
	if test "$#" == 4 ; then
		INFO_N=$4
	fi
	./phiola co -copy -f -s 1 $1 -o $2
	./phiola i $2 | grep -E "$INFO_N"
	./phiola i -peaks $2 | grep -E "$3"
}

test_copy() {
	if ! test -f co.wav ; then
		./phiola rec -rate 48000 -o co.wav -f -u 2
	fi

	if ! test -f fm_wv.wv ; then
		ffmpeg_encode co.wav
	fi

	## Until
	test_copy_until fm_aac.aac     copy_u_aac.m4a        '4[89],...'
	test_copy_until fm_aac.mkv     copy_u_mkv.m4a        '48,...'
	test_copy_until fm_aac.mp4     copy_u_mp4.m4a        '48,...'
	test_copy_until fm_mp3.mkv     copy_u_mp3_mkv.mp3    '4[89],...'
	test_copy_until fm_mp3.mp3     copy_u_mp3.mp3        '4[89],...'
	test_copy_until fm_mp3_320.mp3 copy_u_mp3_320.mp3    '4[89],...'
	test_copy_until fm_opus.mkv    copy_u_opus_mkv.ogg   '4[6789],...'
	test_copy_until fm_opus.ogg    copy_u_opus.ogg       '4[78],...'
	test_copy_until fm_vorbis.mkv  copy_u_vorbis_mkv.ogg '4[78],...'
	test_copy_until fm_vorbis.ogg  copy_u_vorbis.ogg     '..,...'

	## Seek
	## mkv seeking implementation is not precise
	## mp3 copy algorithm implementation doesn't preserve original delay/padding values
	test_copy_seek fm_aac.aac     copy_s_aac.m4a        '4[89],...'
	test_copy_seek fm_aac.mkv     copy_s_mkv.m4a        '4[789],...'
	test_copy_seek fm_aac.mp4     copy_s_mp4.m4a        '5[01],...'
	test_copy_seek fm_mp3.mkv     copy_s_mp3_mkv.mp3    '4[789],...'
	test_copy_seek fm_mp3.mp3     copy_s_mp3.mp3        '5[01],...'
	test_copy_seek fm_mp3_320.mp3 copy_s_mp3_320.mp3    '5[01],...'
	test_copy_seek fm_opus.mkv    copy_s_opus_mkv.ogg   '4[789],...'
	test_copy_seek fm_opus.ogg    copy_s_opus.ogg       '48,...'
	test_copy_seek fm_vorbis.mkv  copy_s_vorbis_mkv.ogg '4[6789],...'
	./phiola co co.wav -o co_vorbis.ogg -f
	test_copy_seek co_vorbis.ogg  copy_s_vorbis.ogg     '4[78],...'

	## Seek + Until
	O=copy_aac.m4a        ; ./phiola co -copy -f -s 1 -u 2 fm_aac.aac    -o $O ; ./phiola pl $O
	O=copy_aac_mkv.m4a    ; ./phiola co -copy -f -s 1 -u 2 fm_aac.mkv    -o $O ; ./phiola pl $O
	O=copy_mp4.m4a        ; ./phiola co -copy -f -s 1 -u 2 fm_aac.mp4    -o $O ; ./phiola pl $O
	O=copy_mp3.mp3        ; ./phiola co -copy -f -s 1 -u 2 fm_mp3.mp3    -o $O ; ./phiola pl $O
	O=copy_mp3_mkv.mp3    ; ./phiola co -copy -f -s 1 -u 2 fm_mp3.mkv    -o $O ; ./phiola pl $O
	O=copy_opus_mkv.ogg   ; ./phiola co -copy -f -s 1 -u 2 fm_opus.mkv   -o $O ; ./phiola pl $O
	O=copy_opus.ogg       ; ./phiola co -copy -f -s 1 -u 2 fm_opus.ogg   -o $O ; ./phiola pl $O
	O=copy_vorbis_mkv.ogg ; ./phiola co -copy -f -s 1 -u 2 fm_vorbis.mkv -o $O ; ./phiola pl $O
	O=copy_vorbis.ogg     ; ./phiola co -copy -f -s 1 -u 2 fm_vorbis.ogg -o $O ; ./phiola pl $O
}

test_danorm() {
	if ! test -f dani.wav ; then
		./phiola rec -u 10 -f -o dani.wav
	fi
	./phiola co -danorm "frame 500 size 15" dani.wav -f -o dan_co.wav ; ./phiola dan_co.wav
	./phiola co -danorm "" dani.wav -f -o dan_co.flac -af int24 ; ./phiola i dan_co.flac | grep 'int24' ; ./phiola dan_co.flac
	# ./phiola co -danorm "" dani.wav -f -o dan_co96k.flac -af int24 -rate 96000 ; ./phiola dan_co96k.flac
	./phiola rec -danorm "frame 500 size 15" -f -o dan_rec.wav     -u 10 ; ./phiola dan_rec.wav
	./phiola rec -danorm "" -f -o dan_rec96k.flac -u 10 -af int24 -rate 96000 ; ./phiola i dan_rec96k.flac | grep 'int24 96000Hz' ; ./phiola dan_rec96k.flac
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

	./phiola list create . -include "./list*.ogg" -o test.m3u
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

test_list_heal() {
	mkdir -p phi_test phi_test/listheal

	echo "#EXTM3U
#EXTINF:123,ARTIST - abs-rel
`pwd`/phi_test/listheal/file-abs.mp3
#EXTINF:123,ARTIST - norm
././/listheal//file-abs.mp3
#EXTINF:123,ARTIST - chg-ext
listheal/file.mp3
#EXTINF:123,ARTIST - chg-dir
listheal/dir1/dir2/file-cd.mp3
#EXTINF:123,ARTIST - chg-dir-ext
listheal/dir1/dir2/file-cde.mp3
#EXTINF:123,ARTIST - abs-out-of-scope
/tmp/listheal/file-oos.mp3" >phi_test/list.m3u

	echo '#EXTM3U
#EXTINF:123,ARTIST - abs-rel
listheal/file-abs.mp3
#EXTINF:123,ARTIST - norm
listheal/file-abs.mp3
#EXTINF:123,ARTIST - chg-ext
listheal/file.ogg
#EXTINF:123,ARTIST - chg-dir
listheal/dir3/file-cd.mp3
#EXTINF:123,ARTIST - chg-dir-ext
listheal/dir3/file-cde.ogg
#EXTINF:123,ARTIST - abs-out-of-scope
/tmp/listheal/file-oos.mp3' >phi_test/list2.m3u

	touch phi_test/listheal/file-abs.mp3
	touch phi_test/listheal/file.ogg
	mkdir -p phi_test/listheal/dir3
	touch phi_test/listheal/dir3/file-cd.mp3
	touch phi_test/listheal/dir3/file-cde.ogg

	./phiola list heal "phi_test/list.m3u"
	diff -Z phi_test/list.m3u phi_test/list2.m3u

	echo '#EXTM3U
#EXTINF:123,ARTIST - unchanged
listheal/file.ogg' >phi_test/list.m3u
	echo '#EXTM3U
#EXTINF:123,ARTIST - unchanged
listheal/file.ogg' >phi_test/list2.m3u
	./phiola list heal "phi_test/list.m3u"
	diff -Z phi_test/list.m3u phi_test/list2.m3u

	rm -rf phi_test/listheal phi_test/list*.m3u
	rmdir phi_test
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
FILE "rec6.wav" WAVE
 TRACK 04 AUDIO
  TITLE T4
  INDEX 01 00:01:00
 TRACK 05 AUDIO
  TITLE T5
  INDEX 01 00:04:00
EOF
	./phiola i cue.cue | grep 'A1 - T1'
	./phiola i cue.cue | grep 'Artist - T2'
	./phiola i cue.cue | grep 'Artist - T3'
	./phiola i cue.cue | grep 'Artist - T4'
	./phiola i cue.cue | grep 'Artist - T5'
	./phiola cue.cue
	if ./phiola i cue.cue -tracks 2,3 | grep 'A1 - T1' ; then
		false
	fi
	./phiola i cue.cue -tracks 2,3 | grep 'Artist - T2'
	./phiola i cue.cue -tracks 2,3 | grep 'Artist - T3'
	./phiola i cue.cue -tracks 1-3,4

	cat <<EOF >cue.cue
PERFORMER Artist
FILE "rec6.wav" WAVE
 TRACK 01 AUDIO
  PERFORMER A1
  TITLE T1
  INDEX 00 00:00:00
  INDEX 01 00:01:00
 TRACK 02 AUDIO
  TITLE T2
  INDEX 00 00:02:00
  INDEX 01 00:03:00
 TRACK 03 AUDIO
  TITLE T3
  INDEX 00 00:04:00
  INDEX 01 00:05:00
EOF
	./phiola co cue.cue                    -o cue_@tracknumber.wav -f ; ./phiola i cue_01.wav | grep '0:02.000' ; ./phiola i cue_02.wav | grep '0:02.000' ; ./phiola i cue_03.wav | grep '0:01.000'
	./phiola co cue.cue -cue_gaps previous -o cue_@tracknumber.wav -f ; ./phiola i cue_01.wav | grep '0:03.000' ; ./phiola i cue_02.wav | grep '0:02.000' ; ./phiola i cue_03.wav | grep '0:01.000'
	./phiola co cue.cue -cue_gaps current  -o cue_@tracknumber.wav -f ; ./phiola i cue_01.wav | grep '0:02.000' ; ./phiola i cue_02.wav | grep '0:02.000' ; ./phiola i cue_03.wav | grep '0:02.000'
	./phiola co cue.cue -cue_gaps skip     -o cue_@tracknumber.wav -f ; ./phiola i cue_01.wav | grep '0:01.000' ; ./phiola i cue_02.wav | grep '0:01.000' ; ./phiola i cue_03.wav | grep '0:01.000'
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
	if ! test -f http.ogg ; then
		./phiola rec -u 2 -m "title=mytrack" -o http.ogg
		ffmpeg -i http.ogg -y -metadata title=mytrack -c:a libmp3lame http.mp3 2>/dev/null
	fi

	./phiola pl "http://localhost:1/" || true # no connection
	# echo 'application/vnd.apple.mpegurl m3u8' >> $(dirname $(which netmill))/content-types.conf
	netmill http l 8080 w . &
	local nml_pid=$!
	sleep .5

	./phiola pl "http://localhost:8080/404" || true # http error
	./phiola pl "http://localhost:8080/http.ogg"
	./phiola pl "http://localhost:8080/http.mp3"

	# playlist via HTTP
	echo "http://localhost:8080/http.ogg" >./http.m3u
	./phiola pl "http://localhost:8080/http.m3u"

	# -tee
	./phiola pl "http://localhost:8080/http.ogg" -tee @stdout.ogg >http-tee-stdout.ogg ; ./phiola http-tee-stdout.ogg
	./phiola pl "http://localhost:8080/http.ogg" -tee http-tee.ogg ; ./phiola http-tee.ogg
	./phiola pl "http://localhost:8080/http.ogg" -tee http-tee.ogg # file already exists
	# ./phiola pl "http://localhost:8080/http.ogg" -tee http-@title.ogg ; ./phiola http-mytrack.ogg

	# -dup
	./phiola pl "http://localhost:8080/http.mp3" -dup @stdout.wav >http-dup-stdout.wav ; ./phiola http-dup-stdout.wav
	./phiola pl "http://localhost:8080/http.mp3" -dup http-dup-@title.wav ; ./phiola http-dup-mytrack.wav

	# HLS
	cp http.ogg hls1.ogg
	cp http.ogg hls2.ogg
	cp http.ogg hls3.ogg
	cat <<EOF >hls.m3u8
#EXTM3U
#EXT-X-MEDIA-SEQUENCE:1
hls1.ogg
hls2.ogg
hls3.ogg
EOF
	./phiola pl "http://localhost:8080/hls.m3u8" &
	sleep 10
	kill $!

	kill $nml_pid
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
	CHILD=$(./phiola -Background rec -f -o rec_remote.flac -remote)
	sleep 5
	./phiola remote stop
	sleep 1
	ps -q $CHILD && exit 1 # subprocess must exit
	./phiola remote stop || true
	./phiola i rec_remote.flac
}

test_tag() {
	if ! test -f tag.mp3 ; then
		./phiola rec -o tag.wav -f -u 2
		ffmpeg -i tag.wav -y -c:a libmp3lame tag.mp3 2>/dev/null
		./phiola co tag.wav -o .ogg
		./phiola co tag.wav -o .opus
	fi

	# add new tags
	./phiola tag -m 'artist=Great Artist' -m 'title=Cool Song' tag.mp3 tag.ogg tag.opus
	./phiola i tag.mp3 | grep "Great Artist - Cool Song"
	./phiola i tag.ogg | grep "Great Artist - Cool Song"
	./phiola i tag.opus | grep "Great Artist - Cool Song"

	# replace tag
	./phiola tag -m 'title=Very Cool Song' tag.mp3 tag.ogg tag.opus
	./phiola i tag.mp3 | grep "Great Artist - Very Cool Song"
	./phiola i tag.ogg | grep "Great Artist - Very Cool Song"
	./phiola i tag.opus | grep "Great Artist - Very Cool Song"

	# set tag
	./phiola tag -clear -m 'title=Cool Song' tag.mp3 tag.ogg tag.opus
	./phiola i tag.mp3 | grep " - Cool Song"
	./phiola i tag.ogg | grep " - Cool Song"
	./phiola i tag.opus | grep " - Cool Song"
}

test_clean() {
	rm -f *.wav *.flac *.m4a *.ogg *.opus *.mp3 fm_* ofv/*.ogg *.cue *.m3u copa/*
	rmdir ofv copa
}

TESTS=(
	device
	record
	record_split
	# record_manual
	play
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
	dir_read
	list
	list_heal
	# list_manual
	cue
	ofile_vars
	remote
	tag
	# http
	clean
	# rec_play_alsa
	# wasapi_exclusive
	# wasapi_loopback
	help
	)

if test "$#" -gt "0" ; then
	TESTS=("$@")
fi

for T in "${TESTS[@]}" ; do
	test_$T
done

echo DONE
