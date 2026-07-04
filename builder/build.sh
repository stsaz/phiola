#!/bin/bash
# phiola release builder

if [[ $# -ne 2 ]]; then
	echo "Usage: build.sh OS-CPU TAG"
	exit 1
fi

set -xeu
TARGET="$1"
TAG="$2"

# "v1.0" -> "1.0"
VER="${TAG#v}"

case "$TARGET" in
	linux-amd64)
		;;

	linux-arm64)
		export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
		ARGS="CPU=arm64 PHI_HTTP_SSL=0"
		;;

	windows-amd64)
		ARGS="OS=windows"
		;;

	*)
		exit 1
		;;
esac

export PATH="$PATH:/usr/lib/llvm-19/bin"
mkdir -p _$TARGET
make -j$(nproc) \
	-C _$TARGET \
	-f ../Makefile \
	ROOT_DIR=../.. \
	COMPILER=clang \
	CFLAGS_USER=-fno-diagnostics-color \
	PHI_VERSION_STR="$VER" \
	PKG_VER="$VER" \
	$ARGS \
	release
