#!/bin/bash

# phiola test: network functions

test_server() {
	if [[ ! -f sv.flac ]]; then
		./phiola rec -u 2 -m "title=mytrack" -o sv.flac
	fi

	./phiola server sv.flac -shuffle -opus_q 64 -max_cl 10000 &
	sleep .1
	./phiola pl http://127.0.0.1:21014/ -u 3
	kill -9 $!
	sleep .1

	./phiola server sv.flac -aac_q 64 &
	sleep .1
	./phiola pl http://127.0.0.1:21014/ -u 3
	kill -9 $!
}

test_http() {
	if [[ ! -f http.ogg ]]; then
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
	./phiola pl "http://localhost:8080/http.ogg" -tee @stdout.ogg >http-tee-stdout.ogg
	./phiola http-tee-stdout.ogg

	./phiola pl "http://localhost:8080/http.ogg" -tee http-tee.ogg
	./phiola http-tee.ogg

	./phiola pl "http://localhost:8080/http.ogg" -tee http-tee.ogg # file already exists
	# ./phiola pl "http://localhost:8080/http.ogg" -tee http-@title.ogg ; ./phiola http-mytrack.ogg

	# -dup
	./phiola pl "http://localhost:8080/http.mp3" -dup @stdout.wav >http-dup-stdout.wav
	./phiola http-dup-stdout.wav

	./phiola pl "http://localhost:8080/http.mp3" -dup http-dup-@title.wav
	./phiola http-dup-mytrack.wav

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

test_https() {
	./phiola pl https://example.org
}
