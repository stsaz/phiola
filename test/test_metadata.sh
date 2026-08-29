#!/bin/bash

test_output_vars() {
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

test_meta_inject() {
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
