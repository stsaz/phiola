#!/bin/bash

# phiola: cross-build on Linux for Android

# ANDROID_HOME=
# ANDROID_CLT_URL=
# ANDROID_BT_VER=
# ANDROID_PF_VER=
# ANDROID_NDK_VER=
# GRADLE_DIR=
# CPU=
IMAGE_NAME=phiola-android-builder
CONTAINER_NAME=phiola_android_build
JOBS=${JOBS:-$(nproc)}
ARGS=${@@Q}

set -xe

test -d "../phiola"
PHI_DIR=$(pwd)

path_basename() {
	echo "${1##*/}"
}

if test -z "$ANDROID_HOME" ; then
	exit 1
elif ! test -d "$ANDROID_HOME/cmdline-tools" ; then
	# Download and unpack Android tools
	mkdir -p /tmp/android-dl
	cd /tmp/android-dl
	if test -z "$ANDROID_CLT_URL" ; then
		exit 1
	fi
	wget $ANDROID_CLT_URL

	cd $ANDROID_HOME
	mkdir cmdline-tools
	cd cmdline-tools
	unzip /tmp/android-dl/$(path_basename "$ANDROID_CLT_URL")
	mv cmdline-tools latest
	cd $PHI_DIR
fi

if ! test -d "$ANDROID_HOME/platforms/android-$ANDROID_PF_VER" ; then
	# Download and install Android SDK
	cd $ANDROID_HOME/cmdline-tools/latest/bin
	./sdkmanager --list
	if test -z "$ANDROID_PF_VER" ; then
		exit 1
	elif test -z "$ANDROID_BT_VER" ; then
		exit 1
	elif test -z "$ANDROID_NDK_VER" ; then
		exit 1
	fi
	./sdkmanager \
	 "platform-tools" \
	 "platforms;android-$ANDROID_PF_VER" \
	 "build-tools;$ANDROID_BT_VER" \
	 "ndk;$ANDROID_NDK_VER"
	cd $PHI_DIR
fi

image() {
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
 openjdk-21-jdk
EOF
}

if ! podman container exists $CONTAINER_NAME ; then
	if ! podman image exists $IMAGE_NAME ; then
		# Create builder image
		image
	fi

	if test -z "$GRADLE_DIR" ; then
		exit 1
	fi

	# Create builder container
	podman create --attach --tty \
	 -v $(pwd)/..:/src \
	 -v $ANDROID_HOME:/Android \
	 -v $GRADLE_DIR:/root/.gradle \
	 --workdir /src/phiola \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 sleep 3600
fi

if ! podman container top $CONTAINER_NAME ; then
	# Start container in background
	podman start --attach $CONTAINER_NAME &
	while ! podman container top $CONTAINER_NAME ; do
		sleep .5
	done
fi

# Prepare build script

ODIR=_android-$CPU

cat >build_android.sh <<EOF
set -xe

export PATH=/Android/ndk/$ANDROID_NDK_VER/toolchains/llvm/prebuilt/linux-x86_64/bin:\$PATH
export ANDROID_NDK_ROOT=/Android/ndk/$ANDROID_NDK_VER
export ANDROID_HOME=/Android
mkdir -p $ODIR
make -j$JOBS \
 -C _android-$CPU \
 -f ../android/Makefile \
 -I ../android \
 COMPILER=clang \
 CPU=$CPU \
 ROOT_DIR=../.. \
 NDK_DIR=/Android/ndk/$ANDROID_NDK_VER \
 $ARGS
EOF

# Build inside the container
podman exec $CONTAINER_NAME \
 bash build_android.sh
