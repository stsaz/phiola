#!/bin/bash

# phiola: cross-build on Linux for Linux/(AMD64|ARM64) | Windows/AMD64

IMAGE_NAME=phiola-debianbw-builder
CONTAINER_NAME=phiola_debianBW_build
BUILD_TARGET=linux
if test "$CPU" == "arm64" ; then
	IMAGE_NAME=phiola-debianbw-arm64-builder
	CONTAINER_NAME=phiola_debianBW_arm64_build
fi
if test "$OS" == "windows" ; then
	IMAGE_NAME=phiola-win64-builder
	CONTAINER_NAME=phiola_win64_build
	BUILD_TARGET=mingw64
fi
ARGS=${@@Q}

if test "$JOBS" == "" ; then
	JOBS=8
fi

set -xe

if ! test -d "../phiola" ; then
	exit 1
fi

if ! podman container exists $CONTAINER_NAME ; then
	if ! podman image exists $IMAGE_NAME ; then

		# Create builder image
		if test "$OS" == "windows" ; then

			cat <<EOF | podman build -t $IMAGE_NAME -f - .
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
 curl
RUN apt install -y \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
EOF

		elif test "$CPU" == "arm64" ; then

			cat <<EOF | podman build -t $IMAGE_NAME -f - .
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
 curl
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

		else

			cat <<EOF | podman build -t $IMAGE_NAME -f - .
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
 curl
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
		fi
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 bash -c "cd /src/phiola && source ./build_$BUILD_TARGET.sh"
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

ODIR=_linux-amd64
ARGS_OS=""
ARGS_PHI=""
ENV_CPU=""
FFPACK_TARGETS=zstd
SSL_DISABLE=0

if test "$CPU" == "arm64" ; then
	ODIR=_linux-arm64
	ARGS_OS="CPU=arm64 \
CROSS_PREFIX=aarch64-linux-gnu-"
	ENV_CPU="export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig"
	ARGS_PHI="PHI_HTTP_SSL=0"
	SSL_DISABLE=1
fi

if test "$OS" == "windows" ; then
	ODIR=_windows-amd64
	ARGS_OS="OS=windows \
COMPILER=gcc \
CROSS_PREFIX=x86_64-w64-mingw32-"
	FFPACK_TARGETS="zstd zlib"
fi

cat >build_$BUILD_TARGET.sh <<EOF
set -xe

if test "$SSL_DISABLE" != "1" ; then
	mkdir -p ../netmill/3pt/$ODIR
	make -j$JOBS openssl \
	 -C ../netmill/3pt/$ODIR \
	 -f ../Makefile \
	 -I .. \
	 $ARGS_OS
fi

mkdir -p ../ffpack/$ODIR
make -j$JOBS $FFPACK_TARGETS \
 -C ../ffpack/$ODIR \
 -f ../Makefile \
 -I .. \
 $ARGS_OS

mkdir -p alib3/$ODIR
make -j$JOBS \
 -C alib3/$ODIR \
 -f ../Makefile \
 -I .. \
 $ARGS_OS

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
EOF

# Build inside the container
podman exec $CONTAINER_NAME \
 bash -c "cd /src/phiola && source ./build_$BUILD_TARGET.sh"
