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

	./phiola server sv.flac -ssl mod/http-client.pem &
	sleep .1
	./phiola pl https://127.0.0.1:21014/ -u 3
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

test_http_ctl() {
	if [[ ! -f httpctl.ogg ]]; then
		./phiola rec -u 2 -m "title=mytrack" -o httpctl.ogg
		cp httpctl.ogg httpctl2.ogg
		cp httpctl.ogg httpctl3.ogg
	fi

	local base=http://127.0.0.1:21014
	local cookie=phiola-cookies.txt
	rm -f "$cookie"

	./phiola pl -remote_proto http -remote_pass test123 httpctl*.ogg &
	local pid=$!
	sleep 1

	curl -s "$base/" > /dev/null
	curl -s "$base/login.html" > /dev/null

	# API requires auth
	[[ $(curl -s -o /dev/null -w "%{http_code}" "$base/play/status") == 401 ]]

	# wrong password
	[[ $(curl -s -o /dev/null -w "%{http_code}" -X POST "$base/login" -d '{"pass":"wrong"}') == 403 ]]

	# correct password
	[[ $(curl -s -c "$cookie" -o /dev/null -w "%{http_code}" -X POST "$base/login" -d '{"pass":"test123"}') == 200 ]]

	# authenticated API calls
	[[ $(curl -s -b "$cookie" -o /dev/null -w "%{http_code}" "$base/play/status") == 200 ]]
	curl -s -b "$cookie" "$base/play/status"
	curl -s -b "$cookie" -X POST "$base/play/play"
	curl -s -b "$cookie" -X POST "$base/play/pause"
	curl -s -b "$cookie" -X POST "$base/play/seek" -d '{"pos_pct":50}'
	curl -s -b "$cookie" -X POST "$base/play/volume" -d '{"vol_pct":80}'
	curl -s -b "$cookie" -X POST "$base/play/next"
	curl -s -b "$cookie" -X POST "$base/play/prev"
	curl -s -b "$cookie" -X POST "$base/play/stop"
	curl -s -b "$cookie" "$base/notfound"

	ps -q "$pid" || exit 1
	kill "$pid" || true

	# HTTPS
	./phiola pl -remote_proto http -ssl mod/http-client.pem -remote_pass test123 httpctl*.ogg &
	local pid=$!
	sleep 1
	base=https://127.0.0.1:21014
	[[ $(curl -sk -o /dev/null -w "%{http_code}" "$base/play/status") == 401 ]]
	rm -f "$cookie"
	[[ $(curl -sk -c "$cookie" -o /dev/null -w "%{http_code}" -X POST "$base/login" -d '{"pass":"test123"}') == 200 ]]
	[[ $(curl -sk -b "$cookie" -o /dev/null -w "%{http_code}" "$base/play/status") == 200 ]]
	ps -q "$pid" || exit 1
	kill "$pid" || true
	rm -f "$cookie"
}
