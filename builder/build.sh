#!/bin/bash
# phiola release builder

if [[ $# -ne 2 ]]; then
	echo "Usage: build.sh OS-CPU|TARGET TAG"
	exit 1
fi

set -xeu
TARGET="$1"
TAG="$2"

# "v1.0" -> "1.0"
VER="${TAG#v}"

case "$TARGET" in
	android)
		mkdir -p _$TARGET
		# APK_KEY_STORE=
		# APK_KEY_PASS=
		make -j$(nproc) \
			-C _$TARGET \
			-f /src/phiola/android/Makefile \
			-I /src/phiola/android \
			ROOT_DIR=/src \
			CPU=arm64 \
			PHI_VERSION_STR="$VER" \
			APK_VER="$VER" \
			release
		exit 0
		;;

	macos-arm64)
		mkdir -p _$TARGET
		ROOT="$(pwd)/.."
		gmake -j8 \
			-C _$TARGET \
			-f "$ROOT/phiola/Makefile" \
			ROOT_DIR="$ROOT" \
			CPU=arm64 \
			PHI_HTTP_SSL=0 \
			PHI_GUI=0 \
			PHI_VERSION_STR="$VER" \
			release
		exit 0
		;;

	linux-amd64)
		ARGS=""
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
	-f /src/phiola/Makefile \
	ROOT_DIR=/src \
	CFLAGS_USER=-fno-diagnostics-color \
	PHI_VERSION_STR="$VER" \
	PKG_VER="$VER" \
	$ARGS \
	release
