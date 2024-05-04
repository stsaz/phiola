#!/bin/bash

# phiola: cross-build on Linux for Debian-bookworm ARM64

set -xe

if ! test -d "../phiola" ; then
	exit 1
fi

if ! podman container exists phiola_debianbookworm_arm64_build ; then
	if ! podman image exists phiola-debianbookworm-arm64-builder ; then
		# Create builder image
		cat <<EOF | podman build -t phiola-debianbookworm-arm64-builder -f - .
FROM debian:bookworm-slim
RUN apt update && \
 apt install -y \
  make
RUN apt install -y \
 gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
RUN apt install -y \
 zstd unzip bzip2 xz-utils \
 cmake patch dos2unix curl \
 autoconf libtool libtool-bin gettext
RUN dpkg --add-architecture arm64 && \
 apt update && \
 apt install -y \
  libasound2-dev:arm64 libpulse-dev:arm64 libjack-dev:arm64 \
  libdbus-1-dev:arm64 \
  libgtk-3-dev:arm64
EOF
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name phiola_debianbookworm_arm64_build \
	 phiola-debianbookworm-arm64-builder \
	 bash -c 'cd /src/phiola && source ./build_linux.sh'
fi

# Prepare build script
cat >build_linux.sh <<EOF
set -xe

make -j8 zstd \
 -C ../ffpack \
 CPU=arm64 \
 CROSS_PREFIX=aarch64-linux-gnu-

make -j8 \
 -C alib3 \
 CPU=arm64 \
 CROSS_PREFIX=aarch64-linux-gnu-

export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
mkdir -p _linux-arm64
make -j8 \
 -C _linux-arm64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 PHI_HTTP_SSL=0 \
 CFLAGS_USER=-fno-diagnostics-color \
 CPU=arm64 \
 CROSS_PREFIX=aarch64-linux-gnu- \
 $@

make -j8 app \
 -C _linux-arm64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 PHI_HTTP_SSL=0 \
 CPU=arm64 \
 CROSS_PREFIX=aarch64-linux-gnu-
EOF

# Build inside the container
podman start --attach phiola_debianbookworm_arm64_build
