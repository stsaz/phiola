#!/bin/bash

# phiola: cross-build for Linux/(AMD64|ARM64) | Windows/AMD64

OS=${OS:-linux}
CPU=${CPU:-amd64}
JOBS=${JOBS:-$(nproc)}
ARGS=${@@Q}

IMAGE_NAME=phiola-debtx-builder
CONTAINER_NAME=phiola_debtx_build
BUILD_TARGET=linux
if [[ "$OS" == "windows" ]]; then
	IMAGE_NAME=phiola-win64-builder
	CONTAINER_NAME=phiola_win64_build
	BUILD_TARGET=mingw64
elif [[ "$CPU" == "arm64" ]]; then
	IMAGE_NAME=phiola-arm64-builder
	CONTAINER_NAME=phiola_arm64_build
fi

NFPM_VER=2.46.3
NFPM_SHA256SUM=d6417f99d5fa32bba7a4e007084615d3897651498c2e443118c26b9ec3b698a8

set -xe

test -d "../phiola"

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
 dos2unix
RUN apt install -y \
 cmake \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 perl
RUN apt install -y \
 clang lld
RUN curl -L -o nfpm_${NFPM_VER}_amd64.deb https://github.com/goreleaser/nfpm/releases/download/v${NFPM_VER}/nfpm_${NFPM_VER}_amd64.deb \
 && echo "${NFPM_SHA256SUM} *nfpm_${NFPM_VER}_amd64.deb" | sha256sum -c - \
 && dpkg -i nfpm_${NFPM_VER}_amd64.deb \
 && rm -f nfpm_${NFPM_VER}_amd64.deb
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
 dos2unix
RUN apt install -y \
 cmake \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 perl
RUN apt install -y \
 clang lld
RUN curl -L -o nfpm_${NFPM_VER}_amd64.deb https://github.com/goreleaser/nfpm/releases/download/v${NFPM_VER}/nfpm_${NFPM_VER}_amd64.deb \
 && echo "${NFPM_SHA256SUM} *nfpm_${NFPM_VER}_amd64.deb" | sha256sum -c - \
 && dpkg -i nfpm_${NFPM_VER}_amd64.deb \
 && rm -f nfpm_${NFPM_VER}_amd64.deb
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
 dos2unix
RUN apt install -y \
 cmake \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 perl
RUN apt install -y \
 clang lld
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
	 sleep 3600
fi

if ! podman container top $CONTAINER_NAME ; then
	# Start container in background
	podman start --attach $CONTAINER_NAME &
	# Wait until the container is ready
	while ! podman container top $CONTAINER_NAME ; do
		sleep .5
	done
fi

# Prepare build script

ODIR=${ODIR:-_$OS-$CPU}
ARGS_OS=""
ARGS_PHI=""
ENV_CPU=""

if [[ "$CPU" == "arm64" ]]; then
	ARGS_OS="CPU=arm64"
	ENV_CPU="export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig"
	ARGS_PHI="PHI_HTTP_SSL=0"
elif [[ "$OS" == "windows" ]]; then
	ARGS_OS="OS=windows"
fi

cat >build_$BUILD_TARGET.sh <<EOF
set -xe

export PATH=$PATH:/usr/lib/llvm-19/bin
$ENV_CPU
mkdir -p $ODIR
make -j$JOBS \
 -C $ODIR \
 -f ../Makefile \
 ROOT_DIR=../.. \
 COMPILER=clang \
 $ARGS_OS \
 CFLAGS_USER=-fno-diagnostics-color \
 $ARGS_PHI \
 $ARGS

# cp -au /usr/lib/x86_64-linux-gnu/libasan.so.8* /src/phiola/_linux-amd64/phiola-2/
EOF

# Build inside the container
podman exec $CONTAINER_NAME \
 bash build_$BUILD_TARGET.sh
