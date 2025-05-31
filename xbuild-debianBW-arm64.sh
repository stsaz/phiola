#!/bin/bash

# phiola: cross-build on Linux for Debian-bookworm ARM64

IMAGE_NAME=phiola-debianbw-arm64-builder
CONTAINER_NAME=phiola_debianBW_arm64_build
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
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 bash -c 'cd /src/phiola && source ./build_linux.sh'
fi

# Prepare build script
cat >build_linux.sh <<EOF
set -xe

mkdir -p ../ffpack/_linux-arm64
make -j$JOBS zstd \
 -C ../ffpack/_linux-arm64 \
 -f ../Makefile \
 -I .. \
 CPU=arm64 \
 CROSS_PREFIX=aarch64-linux-gnu-

mkdir -p alib3/_linux-arm64
make -j$JOBS \
 -C alib3/_linux-arm64 \
 -f ../Makefile \
 -I .. \
 CPU=arm64 \
 CROSS_PREFIX=aarch64-linux-gnu-

export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
mkdir -p _linux-arm64
make -j$JOBS \
 -C _linux-arm64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 PHI_HTTP_SSL=0 \
 CFLAGS_USER=-fno-diagnostics-color \
 CPU=arm64 \
 CROSS_PREFIX=aarch64-linux-gnu- \
 $ARGS
EOF

# Build inside the container
podman start --attach $CONTAINER_NAME
