#!/bin/bash

# phiola: cross-build on Linux for Windows/AMD64

IMAGE_NAME=phiola-win64-builder
CONTAINER_NAME=phiola_win64_build
ARGS=${@@Q}

set -xe

# Ensure we're inside phiola directory
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
 gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
EOF
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 bash -c 'cd /src/phiola && source ./build_mingw64.sh'
fi

# Prepare build script
cat >build_mingw64.sh <<EOF
set -xe

make -j8 openssl \
 -C ../netmill/3pt \
 -f ../Makefile \
 -I .. \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32-

mkdir -p ../ffpack/_windows-amd64
make -j8 zstd \
 -C ../ffpack/_windows-amd64 \
 -f ../Makefile \
 -I .. \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32-

mkdir -p alib3/_windows-amd64
make -j8 \
 -C alib3/_windows-amd64 \
 -f ../Makefile \
 -I .. \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32-

mkdir -p _windows-amd64
make -j8 \
 -C _windows-amd64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32- \
 CFLAGS_USER=-fno-diagnostics-color \
 $ARGS
EOF

# Build inside the container
podman start --attach $CONTAINER_NAME
