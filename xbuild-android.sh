#!/bin/bash

# phiola: cross-build on Linux for Android

set -xe

if ! test -d "../phiola" ; then
	exit 1
fi

if ! podman container exists phiola_android_build ; then
	if ! podman image exists phiola-android-builder ; then
		# Create builder image
		cat <<EOF | podman build -t phiola-android-builder -f - .
FROM debian:bookworm-slim
RUN apt update && \
 apt install -y \
  make
RUN apt install -y \
 zstd zip unzip bzip2 xz-utils \
 perl \
 cmake \
 patch \
 dos2unix \
 curl \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
EOF
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 -v $SDK_DIR:/Android \
	 --name phiola_android_build \
	 phiola-android-builder \
	 bash -c 'cd /src/phiola && source ./build_android.sh'
fi

# Prepare build script
cat >build_android.sh <<EOF
set -xe

make -j8 zstd \
 -C ../ffpack \
 SYS=android \
 CPU=arm64 \
 NDK_DIR=/Android/ndk/$NDK_VER

make -j8 \
 -C alib3 \
 SYS=android \
 CPU=arm64 \
 NDK_DIR=/Android/ndk/$NDK_VER

mkdir -p _android-arm64
make -j8 lib-arm64 \
 -C _android-arm64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 SDK_DIR=/Android \
 NDK_VER=$NDK_VER \
 A_API=26 \
 $@
EOF

# Build inside the container
podman start --attach phiola_android_build
