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
ARGS=${@@Q}

set -xe

if ! test -d "../phiola" ; then
	exit 1
fi
PHI_DIR=$(pwd)

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
	fcom unpack /tmp/android-dl/commandlinetools*
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
 openjdk-17-jdk
EOF
	fi

	if test -z "$GRADLE_DIR" ; then
		exit 1
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 -v $ANDROID_HOME:/Android \
	 -v $GRADLE_DIR:/root/.gradle \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 bash -c 'cd /src/phiola && source ./build_android.sh'
fi

if ! podman container top $CONTAINER_NAME ; then
	cat >build_android.sh <<EOF
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
cat >build_android.sh <<EOF
set -xe

export PATH=/Android/ndk/$ANDROID_NDK_VER/toolchains/llvm/prebuilt/linux-x86_64/bin:\$PATH

if test "$CPU" != "arm" ; then
	export ANDROID_NDK_ROOT=/Android/ndk/$ANDROID_NDK_VER
	mkdir -p ../netmill/3pt/_android-$CPU
	make -j8 openssl \
	 -C ../netmill/3pt/_android-$CPU \
	 -f ../Makefile \
	 -I .. \
	 COMPILER=clang \
	 SYS=android \
	 CPU=$CPU \
	 NDK_DIR=/Android/ndk/$ANDROID_NDK_VER
fi

mkdir -p ../ffpack/_android-$CPU
make -j8 zstd \
 -C ../ffpack/_android-$CPU \
 -f ../Makefile \
 -I .. \
 COMPILER=clang \
 SYS=android \
 CPU=$CPU \
 NDK_DIR=/Android/ndk/$ANDROID_NDK_VER

mkdir -p alib3/_android-$CPU
make -j8 \
 -C alib3/_android-$CPU \
 -f ../Makefile \
 -I .. \
 COMPILER=clang \
 SYS=android \
 CPU=$CPU \
 NDK_DIR=/Android/ndk/$ANDROID_NDK_VER

export ANDROID_HOME=/Android
mkdir -p _android-$CPU
make -j8 \
 -C _android-$CPU \
 -f ../android/Makefile \
 -I ../android \
 COMPILER=clang \
 ROOT_DIR=../.. \
 NDK_VER=$ANDROID_NDK_VER \
 CPU=$CPU \
 A_API=26 \
 $ARGS
EOF

# Build inside the container
podman exec $CONTAINER_NAME \
 bash -c 'cd /src/phiola && source ./build_android.sh'
