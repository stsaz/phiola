#!/bin/bash

# phiola: cross-build on Linux for Linux/(AMD64|ARM64) | Windows/AMD64

if test -z "$OS" ; then
	OS=linux
fi
if test -z "$CPU" ; then
	CPU=amd64
fi

IMAGE_NAME=phiola-debtx-builder
CONTAINER_NAME=phiola_debtx_build
BUILD_TARGET=linux
if test "$OS" == "windows" ; then
	IMAGE_NAME=phiola-win64-debtx-builder
	CONTAINER_NAME=phiola_win64_debtx_build
	BUILD_TARGET=mingw64
fi
if test "$CPU" == "arm64" ; then
	IMAGE_NAME=phiola-arm64-debtx-builder
	CONTAINER_NAME=phiola_arm64_debtx_build
fi

if test -z "$JOBS" ; then
	JOBS=$(nproc)
fi

ARGS=${@@Q}

set -xe

if ! test -d "../phiola" ; then
	exit 1
fi

image_linux_amd64() {
	cat <<EOF | podman build -t $IMAGE_NAME -f - .
FROM debian:trixie-slim
RUN apt update && \
 apt install -y \
  make
RUN apt install -y \
 curl \
 zstd zip unzip bzip2 xz-utils \
 patch \
 perl \
 cmake \
 dos2unix
RUN apt install -y \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 gcc g++
RUN apt install -y \
 libasound2-dev libpulse-dev libjack-dev \
 libdbus-1-dev
RUN apt install -y \
 libgtk-3-dev
EOF
}

image_linux_arm64() {
	cat <<EOF | podman build -t $IMAGE_NAME -f - .
FROM debian:trixie-slim
RUN apt update && \
 apt install -y \
  make
RUN apt install -y \
 curl \
 zstd zip unzip bzip2 xz-utils \
 patch \
 perl \
 cmake \
 dos2unix
RUN apt install -y \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
RUN dpkg --add-architecture arm64 && \
 apt update && \
 apt install -y \
  libasound2-dev:arm64 libpulse-dev:arm64 libjack-dev:arm64 \
  libdbus-1-dev:arm64
RUN apt install -y \
 libgtk-3-dev:arm64
EOF
}

image_windows_amd64() {
	cat <<EOF | podman build -t $IMAGE_NAME -f - .
FROM debian:trixie-slim
RUN apt update && \
 apt install -y \
  make
RUN apt install -y \
 curl \
 zstd zip unzip bzip2 xz-utils \
 patch \
 perl \
 cmake \
 dos2unix
RUN apt install -y \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
EOF
}

if ! podman container exists $CONTAINER_NAME ; then
	if ! podman image exists $IMAGE_NAME ; then
		# Create builder image
		image_${OS}_$CPU
	fi

	# Create builder container
	podman create --attach --tty \
	 -v $(pwd)/..:/src \
	 --workdir /src/phiola \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 bash ./build_$BUILD_TARGET.sh
fi

if ! podman container top $CONTAINER_NAME ; then
	cat >build_$BUILD_TARGET.sh <<EOF
sleep 600
EOF
	# Start container in background
	podman start --attach $CONTAINER_NAME &
	# Wait until the container is ready
	sleep .5
	while ! podman container top $CONTAINER_NAME ; do
		sleep .5
	done
fi

# Prepare build script

if test "$ODIR" == "" ; then
	ODIR=_$OS-$CPU
fi
ARGS_OS=""
ARGS_PHI=""
ENV_CPU=""
FFPACK_TARGETS=zstd
SSL_DISABLE=0

if test "$CPU" == "arm64" ; then
	ARGS_OS="CPU=arm64 \
CROSS_PREFIX=aarch64-linux-gnu-"
	ENV_CPU="export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig"
	ARGS_PHI="PHI_HTTP_SSL=0"
	SSL_DISABLE=1
fi

if test "$OS" == "windows" ; then
	ARGS_OS="OS=windows \
COMPILER=gcc \
CROSS_PREFIX=x86_64-w64-mingw32-"
	FFPACK_TARGETS="zstd zlib"
fi

cat >build_$BUILD_TARGET.sh <<EOF
set -xe

$ENV_CPU
mkdir -p $ODIR
make -j$JOBS \
 -C $ODIR \
 -f ../Makefile \
 ROOT_DIR=../.. \
 $ARGS_OS \
 CFLAGS_USER=-fno-diagnostics-color \
 $ARGS_PHI \
 $ARGS

# cp -au /usr/lib/x86_64-linux-gnu/libasan.so.8* /src/phiola/_linux-amd64/phiola-2/
EOF

# Build inside the container
podman exec $CONTAINER_NAME \
 bash ./build_$BUILD_TARGET.sh
