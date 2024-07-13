#!/bin/bash

# phiola: cross-build on Linux for Windows/AMD64

# LLVM_PATH=
# LLVM_URL=
IMAGE_NAME=phiola-win64-builder
CONTAINER_NAME=phiola_win64_build
ARGS=${@@Q}

set -xe

# Ensure we're inside phiola directory
if ! test -d "../phiola" ; then
	exit 1
fi

# Download and unpack LLVM toolchain
if test -z "$LLVM_PATH" ; then
	exit 1
fi
if ! test -f "$LLVM_PATH/clang" ; then
	mkdir -p /tmp/llvm-dl
	cd /tmp/llvm-dl
	if test -z "$LLVM_URL" ; then
		exit 1
	fi
	wget $LLVM_URL
	fcom unpack llvm*
	mkdir -p $LLVM_PATH
	cp -ar llvm*/* $LLVM_PATH
	cat <<EOF >$LLVM_PATH/clang
unset LINK
/LLVM/bin/clang "\$@"
EOF
	cat <<EOF >$LLVM_PATH/clang++
unset LINK
/LLVM/bin/clang++ "\$@"
EOF
	cd -
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
	 -v $LLVM_PATH:/LLVM \
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
make -j8 zstd zlib \
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

export PATH=/LLVM/bin:\$PATH
mkdir -p _windows-amd64
make -j8 \
 -C _windows-amd64 \
 -f ../Makefile \
 ROOT_DIR=../.. \
 OS=windows \
 CFLAGS_USER='-fno-diagnostics-color' \
 C='/LLVM/clang -c -target x86_64-w64-mingw32' \
 CXX='/LLVM/clang++ -c -target x86_64-w64-mingw32' \
 LINK='/LLVM/clang -target x86_64-w64-mingw32 -rtlib=compiler-rt -fuse-ld=lld' \
 LINKXX='/LLVM/clang++ -target x86_64-w64-mingw32 -rtlib=compiler-rt -fuse-ld=lld -unwindlib=libunwind -stdlib=libc++' \
 $ARGS

# Copy supporting libraries from LLVM package
cp -au /LLVM/x86_64-w64-mingw32/bin/*.dll _windows-amd64/phiola-2
EOF

# Build inside the container
podman start --attach $CONTAINER_NAME

# podman container exec -it $CONTAINER_NAME bash
