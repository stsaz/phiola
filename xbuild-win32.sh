#!/bin/bash

# phiola: cross-build on Linux for Windows/IA32

IMAGE_NAME=phiola-win32-builder
CONTAINER_NAME=phiola_win32_build
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
 gcc-mingw-w64-i686 g++-mingw-w64-i686
EOF
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 bash -c 'cd /src/phiola && source ./build_mingw32.sh'
fi

if ! podman container top $CONTAINER_NAME ; then
	cat >build_mingw32.sh <<EOF
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
cat >build_mingw32.sh <<EOF
set -xe

mkdir -p ../ffpack/_windows-x86
make -j8 zstd \
 -C ../ffpack/_windows-x86 \
 -f ../Makefile \
 -I .. \
 OS=windows \
 CPU=x86 \
 COMPILER=gcc \
 CROSS_PREFIX=i686-w64-mingw32-

mkdir -p alib3/_windows-x86
make -j8 \
 -C alib3/_windows-x86 \
 -f ../Makefile \
 -I .. \
 OS=windows \
 CPU=x86 \
 COMPILER=gcc \
 CROSS_PREFIX=i686-w64-mingw32-

mkdir -p _windows-x86
make -j8 \
 -C _windows-x86 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 PHI_HTTP_SSL=0 \
 OS=windows \
 CPU=x86 \
 COMPILER=gcc \
 CROSS_PREFIX=i686-w64-mingw32- \
 CFLAGS_USER='-fno-diagnostics-color -DFF_WIN_APIVER=0x0502' \
 $ARGS
EOF

# Build inside the container
podman exec $CONTAINER_NAME \
 bash -c 'cd /src/phiola && source ./build_mingw32.sh'
