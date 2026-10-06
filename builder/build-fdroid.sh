#!/bin/bash
# phiola F-Droid builder: `build` step

set -xeu

VER="$(sed -n "s/.*versionName '\(.*\)'.*/\1/p" android/phiola/build.gradle)"
[[ -n "$VER" ]]

# Build libs, APK (unsigned)
# Using system-wide gradle (not local one).
export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"
mkdir -p _android
ROOT_DIR="$(cd .. && pwd)"
PHIOLA="$(pwd)"
make -j$(nproc) \
	-C _android \
	-f "$PHIOLA/android/Makefile" \
	-I "$PHIOLA/android" \
	ROOT_DIR="$ROOT_DIR" \
	PHIOLA="$PHIOLA" \
	CPU=arm64 \
	PHI_HTTP_SSL=0 \
	GRADLE=gradle \
	PHI_VERSION_STR="$VER"
