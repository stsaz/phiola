#!/bin/bash

# phiola: cross-build on Linux for Debian-bullseye

set -xe

if ! test -d "../phiola" ; then
	exit 1
fi

if ! podman container exists phiola_debianbullseye_build ; then
	if ! podman image exists phiola-debianbullseye-builder ; then
		# Create builder image
		cat <<EOF | podman build -t phiola-debianbullseye-builder -f - .
FROM debian:bullseye-slim
RUN apt update && \
 apt install -y \
  make
RUN apt install -y \
 gcc g++
RUN apt install -y \
 zstd unzip bzip2 xz-utils \
 cmake patch dos2unix curl \
 autoconf libtool
RUN apt install -y \
 libgtk-3-dev
RUN apt install -y \
 libasound2-dev libpulse-dev libjack-dev \
 libdbus-1-dev
EOF
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name phiola_debianbullseye_build \
	 phiola-debianbullseye-builder \
	 bash -c 'cd /src/phiola && source ./build_linux.sh'
fi

# Prepare build script
# Note that openssl-3 must be built from source.
cat >build_linux.sh <<EOF
set -xe

make -j8 openssl \
 -C ../netmill/3pt

make -j8 zstd \
 -C ../ffpack

make -j8 \
 -C alib3

mkdir -p _linux-amd64
make -j8 \
 -C _linux-amd64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 CFLAGS_USER=-fno-diagnostics-color \
 $@
make -j8 app \
 -C _linux-amd64 \
 -f ../Makefile \
 ROOT_DIR=../..
EOF

# Build inside the container
podman start --attach phiola_debianbullseye_build
