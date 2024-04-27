# phiola Build Instructions

* Step 1. Download code
* Step 2, Option 1. Cross-Build on Linux
* Step 2, Option 2. Native Build
	* Install dependencies
	* Build
* Build Parameters
* Step 3. Security Check
* Step 4. Use


Supported targets:

* Cross-Build on Linux for AMD64 & ARM64, Windows/AMD64, Android/ARM64
* Native build on Linux

Targets that should work after minor tweaking:

* Build on Windows
* Build on FreeBSD & macOS


## Step 1. Download code

```sh
mkdir phiola-src
cd phiola-src
git clone https://github.com/stsaz/phiola
git clone https://github.com/stsaz/netmill
git clone https://github.com/stsaz/avpack
git clone https://github.com/stsaz/ffaudio
git clone https://github.com/stsaz/ffpack
git clone https://github.com/stsaz/ffgui
git clone https://github.com/stsaz/ffsys
git clone https://github.com/stsaz/ffbase
cd phiola
```


## Step 2, Option 1. Cross-Build on Linux

* Cross-Build on Linux:

	```sh
	bash xbuild-debianbullseye.sh
	```

* Cross-Build on Linux for ARM64:

	```sh
	bash xbuild-debianbookworm-arm64.sh
	```

* Cross-Build on Linux for Windows/AMD64:

	```sh
	bash xbuild-win64.sh
	```

* Cross-Build on Linux for Android/ARM64:

	First, install Android SDK & NDK, and then:

	```sh
	make -j8 zstd \
		-C ../ffpack \
		SYS=android CPU=arm64 NDK_DIR=$SDK_DIR/ndk/YOUR_NDK_VERSION
	make -j8 \
		-C alib3 \
		SYS=android CPU=arm64 NDK_DIR=$SDK_DIR/ndk/YOUR_NDK_VERSION
	make -j8 \
		-C android SDK_DIR=$SDK_DIR
	```


## Step 2, Option 2. Native Build

### Install dependencies

* Debian/Ubuntu

	```sh
	sudo apt install \
		make gcc g++ \
		libasound2-dev libpulse-dev libjack-dev \
		libdbus-1-dev \
		libgtk-3-dev \
		libssl-dev \
		zstd unzip cmake patch dos2unix curl
	```

* Fedora

	```sh
	sudo dnf install \
		make gcc gcc-c++ \
		alsa-lib-devel pulseaudio-libs-devel pipewire-jack-audio-connection-kit-devel \
		dbus-devel \
		gtk3-devel \
		openssl-devel \
		zstd unzip cmake patch dos2unix curl
	```

* Windows

	* msys2 packages: `mingw-w64-clang-x86_64-clang`

	Environment:

	```
	set PATH=c:\clang64\bin;%PATH%
	````

* macOS

	```sh
	brew install \
		make llvm \
		cmake dos2unix
	```

### Build

* Build on Linux:

	```sh
	make -j8 zstd \
		-C ../ffpack
	make -j8 \
		-C alib3
	make -j8
	```

* Build on Windows:

	```sh
	mingw32-make -j8 openssl \
		-C ../netmill/3pt
	mingw32-make -j8 zstd \
		-C ../ffpack
	mingw32-make -j8 \
		-C alib3
	mingw32-make -j8
	```

* Build on FreeBSD & macOS:

	```sh
	gmake -j8 zstd \
		-C ../ffpack
	gmake -j8 \
		-C alib3
	gmake -j8 PHI_HTTP_SSL=0
	```


## Build Parameters

| Parameter | Description |
| --- | --- |
| `DEBUG=1`         | Developer build (no optimization; no strip; all assertions) |
| `ASAN=1`          | Enable ASAN |
| `CFLAGS_USER=...` | Additional C/C++ compiler flags |
| `PHI_CODECS=0`    | Disable all codecs |
| `PHI_HTTP_SSL=0`  | Disable SSL |


## Step 3. Security Check

For security, ensure that the original 3rd party libs were used:

```sh
make hash-check \
	-C ../netmill/3pt
make md5check \
	-C ../ffpack
make md5check \
	-C alib3
```


## Step 4. Use

Directory `phiola-2` is the application directory.  Copy it anywhere you want.
