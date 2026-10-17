# phiola Build Instructions in 3 Steps

Supported targets:
* Cross-build on Linux for AMD64 & ARM64, Windows/AMD64, Android/ARM64
* Native build on macOS

Targets that should work after minor tweaking:
* Native build on Windows
* Native build on FreeBSD


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

* for Linux/AMD64:

	```sh
	bash xbuild.sh
	```

* for Linux/ARM64:

	```sh
	CPU=arm64 \
		bash xbuild.sh
	```

* for Windows/AMD64:

	```sh
	OS=windows \
		bash xbuild.sh
	```

* for Android/ARM64 (Android SDK and Gradle cache directories are passed through):

	```sh
	ANDROID_HOME=$HOME/Android \
	GRADLE_DIR=$HOME/.gradle \
		bash xbuild-android.sh
	```

	Specify these env vars if needed:

		```sh
		ANDROID_CLT_VER=...
		ANDROID_PF_VER=...
		ANDROID_BT_VER=...
		ANDROID_NDK_VER=...
		```

## Step 2, Option 2. Native Build on macOS

```sh
brew install \
	make \
	cmake \
	dos2unix \
	automake libtool \
	ncurses
gmake -j8 PHI_HTTP_SSL=0 PHI_GUI=0
```

## Step 2, Option 3. Native Build on Windows

Install msys2 packages: `mingw-w64-clang-x86_64-clang`

Set environment:

```
set PATH=c:\clang64\bin;%PATH%
````

Build:

```sh
mingw32-make -j8
```

## Step 3. Use

Directory `phiola-2` is the application directory.  Copy it anywhere you want.


## Supported Parameters for `make`

| Parameter | Description |
| --- | --- |
| `DEBUG=1`         | Developer build (no optimization; no strip; all assertions) |
| `ASAN=1`          | Enable ASAN |
| `CFLAGS_USER=...` | Additional C/C++ compiler flags |
| `PHI_CODECS=0`    | Disable all codecs |
| `PHI_GUI=0`       | Disable GUI |
| `PHI_HTTP_SSL=0`  | Disable SSL |
| `PHI_HTTP_SRV=0`  | Disable ICY server |
