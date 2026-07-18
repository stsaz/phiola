#!/bin/bash

# phiola: cross-build on Linux for Android

# ANDROID_HOME=
# ANDROID_CLT_URL=
# ANDROID_BT_VER=
# ANDROID_PF_VER=
# ANDROID_NDK_VER=
# GRADLE_DIR=
# APK_KEY_PASS=
# CPU=
IMAGE_NAME=phiola-android-builder
CONTAINER_NAME=phiola_android_build
JOBS=${JOBS:-$(nproc)}
ARGS=${@@Q}

set -xe

PHIOLA_DIR="$(dirname "$0")"

if [[ -z "$ANDROID_HOME" ]]; then
	exit 1
elif [[ ! -d "$ANDROID_HOME/cmdline-tools" ]]; then
	# Download and unpack Android tools
	if [[ -z "$ANDROID_CLT_URL" ]]; then
		exit 1
	fi
	mkdir -p /tmp/android-dl
	(
		cd /tmp/android-dl
		wget "$ANDROID_CLT_URL"

		cd "$ANDROID_HOME"
		mkdir cmdline-tools
		cd cmdline-tools
		unzip /tmp/android-dl/$(basename "$ANDROID_CLT_URL")
		mv cmdline-tools latest
	)
fi

if [[ ! -d "$ANDROID_HOME/platforms/android-$ANDROID_PF_VER" ]]; then
	# Download and install Android SDK
	if [[ -z "$ANDROID_PF_VER" || -z "$ANDROID_BT_VER" || -z "$ANDROID_NDK_VER" ]]; then
		exit 1
	fi
	# $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --list
	$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager \
	 "platform-tools" \
	 "platforms;android-$ANDROID_PF_VER" \
	 "build-tools;$ANDROID_BT_VER" \
	 "ndk;$ANDROID_NDK_VER"
fi

if ! podman container exists $CONTAINER_NAME ; then
	if ! podman image exists $IMAGE_NAME ; then
		# Create builder image
		podman build -t $IMAGE_NAME -f builder/Dockerfile.android .
	fi

	if [[ -z "$GRADLE_DIR" ]]; then
		exit 1
	fi

	# Create builder container
	podman create --attach --tty \
	 -v "$(pwd)":/build \
	 -v "$PHIOLA_DIR/..":/src \
	 -v $ANDROID_HOME:/Android \
	 -v $GRADLE_DIR:/root/.gradle \
	 --workdir /build \
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

cat >build.sh <<EOF
set -xe

export PATH=/Android/ndk/$ANDROID_NDK_VER/toolchains/llvm/prebuilt/linux-x86_64/bin:\$PATH
export ANDROID_NDK_ROOT=/Android/ndk/$ANDROID_NDK_VER
export ANDROID_HOME=/Android
mkdir -p $ODIR
make -j$JOBS \
 -C _android-$CPU \
 -f /src/phiola/android/Makefile \
 -I /src/phiola/android \
 ROOT_DIR=/src \
 COMPILER=clang \
 CPU=$CPU \
 NDK_DIR=/Android/ndk/$ANDROID_NDK_VER \
 $ARGS
EOF

# Build inside the container
podman exec -e APK_KEY_PASS="$APK_KEY_PASS" $CONTAINER_NAME \
 bash build.sh
