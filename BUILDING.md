# phiola Build Instructions

## Step 1. Install dependencies

* Debian/Ubuntu

	```sh
	sudo apt install \
		git make cmake gcc g++ patch dos2unix curl \
		libdbus-1-dev \
		libasound2-dev libpulse-dev libjack-dev
	```

* Fedora

	```sh
	sudo dnf install \
		git make cmake gcc gcc-c++ patch dos2unix curl \
		dbus-devel \
		alsa-lib-devel pulseaudio-libs-devel pipewire-jack-audio-connection-kit-devel
	```

* macOS

	```sh
	brew install \
		git make cmake llvm dos2unix
	```


## Step 2. Download code

```sh
mkdir phiola-src
cd phiola-src
git clone https://github.com/stsaz/phiola
git clone https://github.com/stsaz/avpack
git clone https://github.com/stsaz/ffaudio
git clone https://github.com/stsaz/ffpack
git clone https://github.com/stsaz/ffos
git clone https://github.com/stsaz/ffbase
cd phiola
```


## Step 3. Build

* Build on Linux:

	```sh
	make -j8 -C ../ffpack libzstd
	make -j8 -C alib3
	make -j8
	```

* Build on Linux for Windows:

	```sh
	make -j8 -C ../ffpack libzstd OS=windows
	make -j8 -C alib3 OS=windows
	make -j8 OS=windows
	```

* Build on Linux for Android/ARM64:

	```sh
	make -j8 -C ../ffpack libzstd NDK_DIR=$SDK_DIR/ndk/YOUR_NDK_VERSION SYS=android CPU=arm64
	make -j8 -C alib3 NDK_DIR=$SDK_DIR/ndk/YOUR_NDK_VERSION SYS=android CPU=arm64
	make -j8 -C android SDK_DIR=$SDK_DIR
	```

* Build on FreeBSD & macOS:

	```sh
	gmake -j8 -C ../ffpack libzstd
	gmake -j8 -C alib3
	gmake -j8
	```

* Build parameters

	* `DEBUG=1` - developer build
	* `PHI_CODECS=0` - disable all codecs


## Step 4. Use

Directory `phiola-2` is the application directory.  Copy it anywhere you want.
