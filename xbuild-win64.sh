#!/bin/bash

# phiola: cross-build on Linux for Windows/AMD64

set -xe

if ! test -d "../phiola" ; then
	exit 1
fi

if ! podman container exists phiola_win64_build ; then
	if ! podman image exists phiola-win64-builder ; then
		# Create builder image
		cat <<EOF | podman build -t phiola-win64-builder -f - .
FROM debian:bookworm-slim AS cxx-mingw64-debian-bookworm
RUN apt update && \
 apt install -y \
  gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 make

FROM cxx-mingw64-debian-bookworm
RUN apt install -y \
 perl \
 zstd zip unzip bzip2 \
 cmake patch dos2unix curl
EOF
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name phiola_win64_build \
	 phiola-win64-builder \
	 bash -c 'cd /src/phiola && source ./build_mingw64.sh'
fi

# Prepare build script
cat >build_mingw64.sh <<EOF
set -xe

make -j8 openssl \
 -C ../netmill/3pt \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32-

make -j8 libzstd \
 -C ../ffpack \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32-

make -j8 \
 -C alib3 \
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
 $@
make -j8 app \
 -C _windows-amd64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32-
EOF

# Build inside the container
podman start --attach phiola_win64_build
