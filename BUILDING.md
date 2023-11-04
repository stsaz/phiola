# phiola Build Instructions

1. Install dependencies.
2. Download code.
3. Build.

	Supported targets:

	* Build on Linux
	* Cross-Build on Linux for Debian-buster
	* Cross-Build on Linux for Windows/AMD64
	* Cross-Build on Linux for Android/ARM64

	Targets that should work after minor tweaking:

	* Build on Windows
	* Build on FreeBSD & macOS


## Step 1. Install dependencies

* Debian/Ubuntu

	```sh
	sudo apt install \
		git \
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
		git \
		make gcc gcc-c++ \
		alsa-lib-devel pulseaudio-libs-devel pipewire-jack-audio-connection-kit-devel \
		dbus-devel \
		gtk3-devel \
		openssl-devel \
		zstd unzip cmake patch dos2unix curl
	```

* Windows

	* `git`
	* msys2 packages: `mingw-w64-clang-x86_64-clang`

	Environment:

	```
	set PATH=c:\clang64\bin;%PATH%
	````

* macOS

	```sh
	brew install \
		git \
		make llvm \
		cmake dos2unix
	```


## Step 2. Download code

```sh
mkdir phiola-src
cd phiola-src
git clone https://github.com/stsaz/phiola
git clone https://github.com/stsaz/netmill
git clone https://github.com/stsaz/avpack
git clone https://github.com/stsaz/ffaudio
git clone https://github.com/stsaz/ffpack
git clone https://github.com/stsaz/ffgui
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

* Cross-Build on Linux for Debian-buster:

> Note that openssl-3 must be built from source.

```sh
cat >build_linux.sh <<EOF
set -e
cd /src/phiola
make -j8 -C ../netmill/3pt openssl
make -j8 -C ../ffpack libzstd
make -j8 -C alib3
make -j8
EOF

cat >builder_debian_buster.Dockerfile <<EOF
FROM debian:buster-slim AS cxx_debian_buster
RUN apt update && \
 apt install -y \
  gcc g++ make

FROM cxx_debian_buster
RUN apt install -y \
 libasound2-dev libpulse-dev libjack-dev \
 libdbus-1-dev \
 libgtk-3-dev \
 zstd unzip cmake patch dos2unix curl
EOF

podman build \
 -t phiola_builder_debian_buster \
 -f builder_debian_buster.Dockerfile .
podman create -a -i -t \
 -v `pwd`/..:/src \
 --name phiola_build_debian_buster \
 phiola_builder_debian_buster \
 bash /src/phiola/build_linux.sh
podman start -a -i phiola_build_debian_buster
```

* Cross-Build on Linux for Windows/AMD64:

```sh
cat >build_mingw64.sh <<EOF
set -e
cd /src/phiola
make -j8 OS=windows -C ../netmill/3pt openssl
make -j8 OS=windows COMPILER=gcc CROSS_PREFIX=x86_64-w64-mingw32- -C ../ffpack libzstd
make -j8 OS=windows COMPILER=gcc CROSS_PREFIX=x86_64-w64-mingw32- -C alib3
make -j8 OS=windows COMPILER=gcc CROSS_PREFIX=x86_64-w64-mingw32-
EOF

cat >builder_mingw64_debian_bookworm.Dockerfile <<EOF
FROM debian:bookworm-slim AS cxx_mingw64_debian_bookworm
RUN apt update && \
 apt install -y \
  gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 make

FROM cxx_mingw64_debian_bookworm
RUN apt install -y \
 perl \
 zstd unzip cmake patch dos2unix curl
EOF

podman build -t phiola_builder_windows64_debian_bookworm -f builder_mingw64_debian_bookworm.Dockerfile .
podman create -a -i -t \
 -v `pwd`/..:/src \
 --name phiola_build_windows64_debian_bookworm \
 phiola_builder_windows64_debian_bookworm \
 bash /src/phiola/build_mingw64.sh
podman start -a -i phiola_build_windows64_debian_bookworm
```

* Cross-Build on Linux for Android/ARM64:

	```sh
	make -j8 SYS=android CPU=arm64 NDK_DIR=$SDK_DIR/ndk/YOUR_NDK_VERSION -C ../ffpack libzstd
	make -j8 SYS=android CPU=arm64 NDK_DIR=$SDK_DIR/ndk/YOUR_NDK_VERSION -C alib3
	make -j8 -C android SDK_DIR=$SDK_DIR
	```

* Build on Windows:

	```sh
	mingw32-make -j8 -C ../netmill/3pt openssl
	mingw32-make -j8 -C ../ffpack libzstd
	mingw32-make -j8 -C alib3
	mingw32-make -j8
	```

* Build on FreeBSD & macOS:

	```sh
	gmake -j8 -C ../ffpack libzstd
	gmake -j8 -C alib3
	gmake -j8 PHI_HTTP_SSL=0
	```

* Build parameters

	* `DEBUG=1` - developer build (no optimization; no strip; all assertions)
	* `ASAN=1` - enable ASAN
	* `CFLAGS_USER=...` - C/C++ compiler flags
	* `PHI_CODECS=0` - disable all codecs
	* `PHI_HTTP_SSL=0` - disable SSL


## Step 4. Check

For security, ensure that the original 3rd party libs were used:

```sh
make -C ../netmill/3pt hash-check
make -C ../ffpack md5check
make -C alib3 md5check
```


## Step 5. Use

Directory `phiola-2` is the application directory.  Copy it anywhere you want.
