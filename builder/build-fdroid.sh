#!/bin/bash
# phiola F-Droid builder: `build` step

set -xeu

if [[ $# -ne 1 ]]; then
	echo "Usage: build-fdroid.sh VERSION"
	exit 1
fi
VER="$1"

# Build libs, APK (unsigned)
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
	PHI_VERSION_STR="$VER"
