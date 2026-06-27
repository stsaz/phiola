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

set -xe

test -d "../phiola"

if ! podman container exists $CONTAINER_NAME ; then
	if ! podman image exists $IMAGE_NAME ; then
		# Create builder image
		podman build -t $IMAGE_NAME -f builder/Dockerfile.$OS-$CPU .
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
EOF

# asan:
# export LD_LIBRARY_PATH=/usr/lib/llvm-19/lib/clang/19/lib/linux

# Build inside the container
podman exec $CONTAINER_NAME \
 bash build_$BUILD_TARGET.sh
