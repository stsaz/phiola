#!/bin/bash

# phiola: cross-build on Linux for Debian-bookworm

IMAGE_NAME=phiola-debianBW-builder
CONTAINER_NAME=phiola_debianBW_build
ARGS=${@@Q}

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
 gcc g++
RUN apt install -y \
 libasound2-dev libpulse-dev libjack-dev \
 libdbus-1-dev
RUN apt install -y \
 libgtk-3-dev
EOF
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 bash -c 'cd /src/phiola && source ./build_linux.sh'
fi

if ! podman container top $CONTAINER_NAME ; then
	cat >build_linux.sh <<EOF
sleep 600
EOF
	# Start container in background
	podman start --attach $CONTAINER_NAME &
	sleep .5
	while ! podman container top $CONTAINER_NAME ; do
		sleep .5
	done
fi

# Prepare build script
# Note that openssl-3 must be built from source.
cat >build_linux.sh <<EOF
set -xe

mkdir -p ../netmill/3pt/_linux-amd64
make -j8 openssl \
 -C ../netmill/3pt/_linux-amd64 \
 -f ../Makefile \
 -I ..

mkdir -p ../ffpack/_linux-amd64
make -j8 zstd \
 -C ../ffpack/_linux-amd64 \
 -f ../Makefile \
 -I ..

mkdir -p alib3/_linux-amd64
make -j8 \
 -C alib3/_linux-amd64 \
 -f ../Makefile \
 -I ..

mkdir -p _linux-amd64
make -j8 \
 -C _linux-amd64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 CFLAGS_USER=-fno-diagnostics-color \
 $ARGS
EOF

# Build inside the container
podman exec $CONTAINER_NAME \
 bash -c 'cd /src/phiola && source ./build_linux.sh'
