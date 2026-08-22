#!/bin/bash

# phiola: cross-build on Linux for Android

ANDROID_HOME="${ANDROID_HOME:-$HOME/Android}"
ANDROID_CLT_VER="${ANDROID_CLT_VER:-14742923}"
ANDROID_BT_VER="${ANDROID_BT_VER:-35.0.0}"
ANDROID_PF_VER="${ANDROID_PF_VER:-33}"
ANDROID_NDK_VER="${ANDROID_NDK_VER:-29.0.14206865}"
GRADLE_DIR="${GRADLE_DIR:-$HOME/.gradle}"
CPU="${CPU:-arm64}"
# APK_KEY_PASS=
JOBS=${JOBS:-$(nproc)}
ARGS=${@@Q}

IMAGE_NAME=phiola-android-builder
CONTAINER_NAME=phiola_android_build

set -xe

PHIOLA_DIR="$(dirname "$0")"

if [[ ! -d "$ANDROID_HOME/cmdline-tools" ]]; then
	# Download and unpack Android command-line tools
	mkdir -p $ANDROID_HOME/cmdline-tools
	curl -fsSL -o /tmp/clt.zip \
		"https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_CLT_VER}_latest.zip"
	unzip -q /tmp/clt.zip -d $ANDROID_HOME/cmdline-tools
	mv $ANDROID_HOME/cmdline-tools/cmdline-tools $ANDROID_HOME/cmdline-tools/latest
	rm /tmp/clt.zip
fi

if [[ ! -d "$ANDROID_HOME/platforms/android-$ANDROID_PF_VER" ]]; then
	# Download and install Android SDK
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
		podman build -t $IMAGE_NAME -f builder/Dockerfile.android-local .
	fi

	# Create builder container
	podman create --attach --tty \
	 -v "$(pwd)":/build \
	 -v "$PHIOLA_DIR/..":/src \
	 -v $ANDROID_HOME:/Android \
	 -v $GRADLE_DIR:/root/.gradle \
	 -e ANDROID_HOME=/Android \
	 -e ANDROID_BT_VER=$ANDROID_BT_VER \
	 -e ANDROID_NDK_ROOT=/Android/ndk/$ANDROID_NDK_VER \
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

cat >build.sh <<EOF
set -xe

export PATH=\$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:\$PATH
mkdir -p _android-$CPU
make -j$JOBS \
 -C _android-$CPU \
 -f /src/phiola/android/Makefile \
 -I /src/phiola/android \
 ROOT_DIR=/src \
 CPU=$CPU \
 $ARGS
EOF

# Build inside the container
podman exec -e APK_KEY_PASS="$APK_KEY_PASS" $CONTAINER_NAME \
 bash build.sh
