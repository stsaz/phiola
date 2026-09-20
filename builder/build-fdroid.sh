#!/bin/bash
# phiola F-Droid builder

set -xeu

if [[ $# -ne 1 ]]; then
	echo "Usage: build-fdroid.sh VERSION"
	exit 1
fi
VER="$1"

# Clone dependencies
(
	cd ..
	git clone https://github.com/stsaz/netmill
	git -C netmill checkout 9057306955a03ca634dca9592354f78bbcc28c98
	git clone https://github.com/stsaz/avpack
	git -C avpack checkout 6127fa0ec66a978c76e2e731a8f11980c3951780
	git clone https://github.com/stsaz/ffaudio
	git -C ffaudio checkout 6a951156e68e33d3925076e680ac56478a648c74
	git clone https://github.com/stsaz/ffpack
	git -C ffpack checkout d16dbae8ac965dc1cf279564aac2b571476b9380
	git clone https://github.com/stsaz/ffsys
	git -C ffsys checkout ab3c924a78bc5ee22337e4b17e5c054189f468cf
	git clone https://github.com/stsaz/ffbase
	git -C ffbase checkout a65bb0bcab405e9f292ececfb3af1511d184e63b
)

# Build libs, APK (unsigned)
export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"
mkdir -p _android
make -j$(nproc) \
	-C _android \
	-f ../android/Makefile \
	-I ../android \
	ROOT_DIR=../.. \
	PHIOLA=.. \
	CPU=arm64 \
	PHI_VERSION_STR="$VER"
