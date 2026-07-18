#!/bin/bash

# phiola test: convert/copy

conv__src_af() {
	./phiola co -perf $1 -f -o cosa-$2.wav -af $2 >>cosa.txt
	./phiola pl -u 2 cosa-$2.wav
}

test_convert_samples() {
	>cosa.txt

	./phiola co -perf source.wav -f -o cosa16.flac >>cosa.txt # int16/i -> int16/ni
	./phiola co -perf source.wav -f -o cosa24.flac >>cosa.txt # int16/i -> int24/ni
	./phiola co -perf source.wav -f -o cosa.ogg    >>cosa.txt # int16/i -> float32/ni

	# int16/i ->
	conv__src_af source.wav int24
	conv__src_af source.wav int32
	conv__src_af source.wav float32

	# int24/i ->
	conv__src_af cosa-int24.wav int16
	conv__src_af cosa-int24.wav int32
	conv__src_af cosa-int24.wav float32

	# int32/i ->
	conv__src_af cosa-int32.wav int24
	conv__src_af cosa-int32.wav int32
	conv__src_af cosa-int32.wav float32

	# float32/i ->
	conv__src_af cosa-float32.wav int16
	conv__src_af cosa-float32.wav int24
	conv__src_af cosa-float32.wav int32

	# int16/ni ->
	conv__src_af cosa16.flac int16
	conv__src_af cosa16.flac int24
	conv__src_af cosa16.flac int32
	conv__src_af cosa16.flac float32

	# int24/ni ->
	conv__src_af cosa24.flac int16
	conv__src_af cosa24.flac int24
	conv__src_af cosa24.flac int32
	conv__src_af cosa24.flac float32

	# float32/ni ->
	conv__src_af cosa.ogg int16
	conv__src_af cosa.ogg int24
	conv__src_af cosa.ogg int32
	conv__src_af cosa.ogg float32

	rm cosa*.wav cosa*.flac cosa*.ogg
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

	if [[ ! -f co.wav ]]; then
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
	if [[ ! -f co.wav ]]; then
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

	./phiola co co.wav -f -o co_wav.aac
	./phiola pl co_wav.aac

	convert__from_to wav ogg
	./phiola i co_wav.ogg              | grep -E '96,000 samples'
	./phiola i co_wav.ogg -peaks       | grep '96,000 total'
	./phiola i -u 1 co_wav.ogg -peaks  | grep -E '48,... total'
	./phiola i -s 1 co_wav.ogg -peaks  | grep -E '4[678],... total'

	convert__from_to wav opus
	./phiola i co_wav.opus             | grep -E '96,... samples'
	./phiola i co_wav.opus -peaks      | grep '96,000 total'
	./phiola i -u 1 co_wav.opus -peaks | grep '48,000 total'
	./phiola i -s 1 co_wav.opus -peaks | grep -E '48,000 total'

	convert__from_to wav mp3
	./phiola i co_wav.mp3 -peaks      | grep '96,000 total'
}

test_convert_parallel() {
	if [[ ! -f co.wav ]]; then
		./phiola rec -u 2 -rate 48000 -o co.wav -f
	fi

	if [[ ! -f copa/co99.wav ]]; then
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
	if [[ "$#" == 4 ]]; then
		INFO_N=$4
	fi
	./phiola co -copy -f -u 1 $1 -o $2
	./phiola i $2 | grep -E "$INFO_N"
	./phiola i -peaks $2 | grep -E "$3"
}

test_copy_seek() {
	local INFO_N=$3
	if [[ "$#" == 4 ]]; then
		INFO_N=$4
	fi
	./phiola co -copy -f -s 1 $1 -o $2
	./phiola i $2 | grep -E "$INFO_N"
	./phiola i -peaks $2 | grep -E "$3"
}

test_copy_ogg_ogg() {
	if [[ ! -f co_vorbis.ogg ]]; then
		./phiola co co.wav -o co_vorbis.ogg -f
		./phiola co co.wav -o co_opus.opus -f
	fi

	# Copy ogg -> ogg (Opus)
		I=co_opus.opus
		O=copy_u_opus.ogg
		./phiola -D co -copy -f -u 1 $I -o $O | grep -E 'page |page:'
		./phiola i -peaks $O | grep 'samples'
		# Note: due to Opus preskip value, decoder may cut/skip samples from the last packet, thus making the file length less than was requested

		O=copy_s_opus.ogg
		./phiola -D co -copy -f -s 1 $I -o $O | grep -E 'page |page:'
		./phiola i -peaks $O | grep 'samples'

		O=copy_su_opus.ogg
		./phiola -D co -copy -f -s 1 -u 2 $I -o $O | grep -E 'page |page:'
		./phiola i -peaks $O | grep 'samples'

	# Copy ogg -> ogg (Vorbis)
		I=co_vorbis.ogg
		O=copy_u_vorbis.ogg
		# Note: copy less than 1 second or else the 2nd page will be also copied because 48000 is not divisible by Vorbis packet length
		./phiola -D co -copy -f -u 0.950 $I -o $O | grep -E 'page |page:'
		./phiola i -peaks $O | grep 'samples'

		O=copy_s_vorbis.ogg
		./phiola -D co -copy -f -s 1 $I -o $O | grep -E 'page |page:'
		./phiola i -peaks $O | grep 'samples'
		# Note: the output file length is less than requested when seeking is performed, because the first packet is skipped/delayed by decoder

		O=copy_su_vorbis.ogg
		./phiola -D co -copy -f -s 1 -u 2 $I -o $O | grep -E 'page |page:'
		./phiola i -peaks $O | grep 'samples'
}

test_copy_mkv_ogg() {
	# Copy mkv -> ogg (Opus)
		I=fm_opus.mkv
		O=copy_u_opus_mkv.ogg
		./phiola -D co -copy -f -u 1 $I -o $O | grep -a 'page:'
		./phiola i -peaks $O | grep 'samples'

		O=copy_s_opus_mkv.ogg
		./phiola -D co -copy -f -s 1 $I -o $O | grep -a 'page:'
		./phiola i -peaks $O | grep 'samples'

		O=copy_su_opus_mkv.ogg
		./phiola -D co -copy -f -s 1 -u 2 $I -o $O | grep -a 'page:'
		./phiola i -peaks $O | grep 'samples'

	# Copy mkv -> ogg (Vorbis)
		I=fm_vorbis.mkv
		O=copy_u_vorbis_mkv.ogg
		./phiola -D co -copy -f -u 0.950 $I -o $O | grep -a 'page:'
		./phiola i -peaks $O | grep 'samples'

		O=copy_s_vorbis_mkv.ogg
		./phiola -D co -copy -f -s 1 $I -o $O | grep -a 'page:'
		./phiola i -peaks $O | grep 'samples'

		O=copy_su_vorbis_mkv.ogg
		./phiola -D co -copy -f -s 1 -u 2 $I -o $O | grep -a 'page:'
		./phiola i -peaks $O | grep 'samples'
}

test_copy() {
	if [[ ! -f co.wav ]]; then
		./phiola rec -rate 48000 -o co.wav -f -u 2
	fi

	if [[ ! -f fm_wv.wv ]]; then
		ffmpeg_encode co.wav
	fi

	test_copy_ogg_ogg
	test_copy_mkv_ogg

	## Until
	test_copy_until fm_aac.aac     copy_u_aac.m4a        '4[89],...'
	test_copy_until fm_aac.mkv     copy_u_mkv.m4a        '48,...'
	test_copy_until fm_aac.mp4     copy_u_mp4.m4a        '48,...'
	test_copy_until fm_mp3.mkv     copy_u_mp3_mkv.mp3    '4[89],...'
	test_copy_until fm_mp3.mp3     copy_u_mp3.mp3        '4[89],...'
	test_copy_until fm_mp3_320.mp3 copy_u_mp3_320.mp3    '4[89],...'

	## Seek
	## mkv seeking implementation is not precise
	## mp3 copy algorithm implementation doesn't preserve original delay/padding values
	test_copy_seek fm_aac.aac     copy_s_aac.m4a        '[45][890],...'
	test_copy_seek fm_aac.mkv     copy_s_mkv.m4a        '4[789],...'
	test_copy_seek fm_aac.mp4     copy_s_mp4.m4a        '5[01],...'
	test_copy_seek fm_mp3.mkv     copy_s_mp3_mkv.mp3    '4[789],...'
	test_copy_seek fm_mp3.mp3     copy_s_mp3.mp3        '5[01],...'
	test_copy_seek fm_mp3_320.mp3 copy_s_mp3_320.mp3    '5[01],...'

	## Seek + Until
	O=copy_aac.m4a        ; ./phiola co -copy -f -s 1 -u 2 fm_aac.aac    -o $O ; ./phiola pl $O
	O=copy_aac_mkv.m4a    ; ./phiola co -copy -f -s 1 -u 2 fm_aac.mkv    -o $O ; ./phiola pl $O
	O=copy_mp4.m4a        ; ./phiola co -copy -f -s 1 -u 2 fm_aac.mp4    -o $O ; ./phiola pl $O
	O=copy_mp3.mp3        ; ./phiola co -copy -f -s 1 -u 2 fm_mp3.mp3    -o $O ; ./phiola pl $O
	O=copy_mp3_mkv.mp3    ; ./phiola co -copy -f -s 1 -u 2 fm_mp3.mkv    -o $O ; ./phiola pl $O
}

meta_rec__dst() {
	./phiola rec -rat 48000 -u 1 -m artist='Great Artist' -m title='Cool Song' -f -o $1
	./phiola i $1 | grep 'Great Artist - Cool Song'
}

meta_conv__src_dst() {
	./phiola co -m artist='AA' $1 -f -o $2
	./phiola i $2 | grep 'AA - Cool Song' || false
}

meta_copy__src_dst() {
	./phiola co -copy -m artist='AA' $1 -f -o $2
	./phiola i $2 | grep 'AA - Cool Song' || false
}

test_meta() {
	# Record with meta
	meta_rec__dst meta.flac
	meta_rec__dst meta.m4a
	meta_rec__dst meta.mp3
	meta_rec__dst meta.ogg
	meta_rec__dst meta.opus

	# Convert with meta
	meta_conv__src_dst meta.flac meta2.flac
	meta_conv__src_dst meta.m4a meta2.m4a
	meta_conv__src_dst meta.opus meta2.opus
	meta_conv__src_dst meta.ogg meta2.ogg
	meta_conv__src_dst meta.mp3 meta2.mp3

	# Copy with meta
	meta_copy__src_dst meta.m4a meta2.m4a
	# meta_copy__src_dst meta.opus meta2.opus
	# meta_copy__src_dst meta.ogg meta2.ogg
	meta_copy__src_dst meta.mp3 meta2.mp3

	rm meta*.flac meta*.m4a meta*.opus meta*.ogg meta*.mp3
}

test_cue() {
	if [[ ! -f "rec6.wav" ]]; then
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

	./phiola co cue.cue -o cue_@tracknumber.wav -f
	./phiola i cue_01.wav | grep '0:02.000'
	./phiola i cue_02.wav | grep '0:02.000'
	./phiola i cue_03.wav | grep '0:01.000'

	./phiola co cue.cue -o cue_@tracknumber.wav -f -cue_gaps previous
	./phiola i cue_01.wav | grep '0:03.000'
	./phiola i cue_02.wav | grep '0:02.000'
	./phiola i cue_03.wav | grep '0:01.000'

	./phiola co cue.cue -o cue_@tracknumber.wav -f -cue_gaps current
	./phiola i cue_01.wav | grep '0:02.000'
	./phiola i cue_02.wav | grep '0:02.000'
	./phiola i cue_03.wav | grep '0:02.000'

	./phiola co cue.cue -o cue_@tracknumber.wav -f -cue_gaps skip
	./phiola i cue_01.wav | grep '0:01.000'
	./phiola i cue_02.wav | grep '0:01.000'
	./phiola i cue_03.wav | grep '0:01.000'
}
