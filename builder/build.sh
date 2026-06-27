#!/bin/bash
# phiola release builder

if [[ $# != 2 ]]; then
	echo "Usage: build.sh CPU TAG"
	exit 1
fi

set -xeu
CPU="$1"
TAG="$2"

# "v1.0" -> "1.0"
VER="${TAG#v}"

if [[ "$CPU" == "amd64" ]]; then
	export PATH="$PATH:/usr/lib/llvm-19/bin"
	mkdir -p _linux-amd64
	make -j$(nproc) \
		-C _linux-amd64 \
		-f ../Makefile \
		ROOT_DIR=../.. \
		COMPILER=clang \
		CFLAGS_USER=-fno-diagnostics-color \
		PHI_VERSION_STR="$VER" \
		PKG_VER="$VER" \
		release
else
	export PATH="$PATH:/usr/lib/llvm-19/bin"
	export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
	mkdir -p _linux-arm64
	make -j$(nproc) \
		-C _linux-arm64 \
		-f ../Makefile \
		ROOT_DIR=../.. \
		COMPILER=clang \
		CPU=arm64 \
		CFLAGS_USER=-fno-diagnostics-color \
		PHI_HTTP_SSL=0 \
		PHI_VERSION_STR="$VER" \
		PKG_VER="$VER" \
		release
fi
