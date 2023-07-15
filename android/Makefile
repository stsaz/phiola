# Android project makefile

# Undefine built-in rules, suffixes and variables
MAKEFLAGS += -Rr

# Set utils
SUBMAKE := $(MAKE) -f $(firstword $(MAKEFILE_LIST))
MKDIR := mkdir -p
RM := rm -rf
CP := cp -au
LS := ls -l

# SDK_DIR := /Android/Sdk
SDK_API := 33.0.0
NDK_VER := 25.1.8937393
NDK_DIR := $(SDK_DIR)/ndk/$(NDK_VER)
# ADB_FLAGS := -d

CPU := amd64
APK_ID := com.github.stsaz.phiola
APK := phiola/build/outputs/apk/release/phiola-release-unsigned.apk
ifeq "$(DEBUG)" "1"
	APK_ID := com.github.stsaz.phiola.debug
	APK := phiola/build/outputs/apk/debug/phiola-debug.apk
endif

build:
	$(MAKE) SDK_DIR=$(SDK_DIR) lib-arm64
	$(MAKE) SDK_DIR=$(SDK_DIR) build-java


# Build .apk

GRADLE := ./gradlew
GRADLE_BUILD := assembleRelease
ifeq "$(DEBUG)" "1"
	GRADLE_BUILD := assembleDebug
endif

build-java:
	$(GRADLE) $(GRADLE_BUILD)
	$(LS) $(APK)

clean-java:
	$(GRADLE) clean

apk-list:
	unzip -l $(APK)


# Sign .apk

APK_KEY_STORE :=
APK_KEY_PASS :=
APK_VER := test
APK_SIGNED := phiola-$(APK_VER)-android-$(CPU).apk

sign:
	$(SDK_DIR)/build-tools/$(SDK_API)/zipalign -v -f 4 $(APK) $(APK_SIGNED)
	$(SDK_DIR)/build-tools/$(SDK_API)/apksigner sign --ks $(APK_KEY_STORE) --ks-pass pass:$(APK_KEY_PASS) $(APK_SIGNED)
	$(SDK_DIR)/build-tools/$(SDK_API)/apksigner verify $(APK_SIGNED)

install-signed:
	$(SDK_DIR)/platform-tools/adb $(ADB_FLAGS) install -r $(APK_SIGNED)


# Upload and install .apk on Android device

install:
	$(SDK_DIR)/platform-tools/adb $(ADB_FLAGS) install -r $(APK)

uninstall:
	$(SDK_DIR)/platform-tools/adb $(ADB_FLAGS) uninstall $(APK_ID)

build-install: build
	$(MAKE) install


# Show logs

log:
	$(SDK_DIR)/platform-tools/adb $(ADB_FLAGS) logcat -v time | grep --color=auto --line-buffered '/phiola'

log-error:
	$(SDK_DIR)/platform-tools/adb $(ADB_FLAGS) logcat -v time | grep -E --color=auto --line-buffered '/phiola|W/|E/'

log-clear:
	$(SDK_DIR)/platform-tools/adb $(ADB_FLAGS) logcat -c

log-bt:
	$(SDK_DIR)/platform-tools/adb $(ADB_FLAGS) logcat -v time | $(NDK_DIR)/ndk-stack -sym phiola/libs/x86_64


# Start Android emulator

AVD := @Pixel_6_API_33
emulator:
	$(SDK_DIR)/emulator/emulator -list-avds
	$(SDK_DIR)/emulator/emulator $(AVD)


# Directories
ROOT_DIR := ../..
PHIOLA := $(ROOT_DIR)/phiola
FFAUDIO := $(ROOT_DIR)/ffaudio
AVPACK := $(ROOT_DIR)/avpack
FFPACK := $(ROOT_DIR)/ffpack
FFBASE := $(ROOT_DIR)/ffbase
FFOS := $(ROOT_DIR)/ffos

include $(FFBASE)/test/makeconf
include andk.mk

ALIB3 := $(PHIOLA)/alib3
ALIB3_BIN := $(ALIB3)/_android-$(CPU)
FFPACK_BIN := $(FFPACK)/_android-$(CPU)

LIB_DIR_AMD64 := $(PHIOLA)/android/phiola/libs/x86_64
LIB_DIR_ARM64 := $(PHIOLA)/android/phiola/libs/arm64-v8a
LIB_DIR_ARM := $(PHIOLA)/android/phiola/libs/armeabi-v7a

lib-amd64:
	$(MKDIR) obj-amd64 $(LIB_DIR_AMD64)
	$(SUBMAKE) -C obj-amd64 CPU=amd64 A_API=26 libs

lib-arm64:
	$(MKDIR) obj-arm64 $(LIB_DIR_ARM64)
	$(SUBMAKE) -C obj-arm64 CPU=arm64 A_API=26 libs

clean-lib:
	$(RM) obj-arm64 obj-amd64 $(LIB_DIR_AMD64) $(LIB_DIR_ARM64)


OBJ_DIR := obj-$(CPU)
LIB_DIR := $(LIB_DIR_AMD64)
ifeq "$(CPU)" "arm64"
	LIB_DIR := $(LIB_DIR_ARM64)
else ifeq "$(CPU)" "arm"
	LIB_DIR := $(LIB_DIR_ARM)
endif
CFLAGS := $(A_CFLAGS)
CFLAGS += -DFFBASE_MEM_ASSERT
CFLAGS += -I$(FFBASE)
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-for-loop-analysis
CFLAGS += -g
CFLAGS += -fPIC
ifeq "$(DEBUG)" "1"
	CFLAGS += -DFF_DEBUG -O0 -Werror
else
	CFLAGS += -O3 -fno-strict-aliasing -fvisibility=hidden
endif
CFLAGS += -I$(PHIOLA)/src -I$(FFOS)
LINKFLAGS := $(A_LINKFLAGS) \
	-llog


DEPS := $(PHIOLA)/src/phiola.h \
	$(PHIOLA)/src/track.h


%.o: $(PHIOLA)/src/core/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/core/*.h) \
		$(wildcard $(PHIOLA)/src/util/*.h)
	$(C) $(CFLAGS) $< -o $@
%.o: $(PHIOLA)/src/queue/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/queue/*.h)
	$(C) $(CFLAGS) $< -o $@
%.o: $(PHIOLA)/src/jni/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/jni/*.h)
	$(C) $(CFLAGS) $< -o $@
libphiola.so: \
		phiola-jni.o \
		core.o \
		auto.o \
		dir-read.o \
		file.o\
		qu.o \
		track.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_PTHREAD) -o $@


MODS += aaudio.so
%.o: $(PHIOLA)/src/adev/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/adev/*.h)
	$(C) $(CFLAGS) -I$(FFAUDIO) $< -o $@
ffaudio-aaudio.o: $(FFAUDIO)/ffaudio/aaudio.c
	$(C) -I$(FFAUDIO) $(CFLAGS) $< -o $@
aaudio.so: aaudio.o ffaudio-aaudio.o
	$(LINK) -shared $+ $(LINKFLAGS) -laaudio -o $@


MODS += afilter.so
%.o: $(PHIOLA)/src/afilter/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/afilter/*.h)
	$(C) $(CFLAGS) $< -o $@
crc.o: $(PHIOLA)/3pt/crc/crc.c
	$(C) $(CFLAGS) $< -o $@
afilter.so: afilter.o \
		crc.o \
		peaks.o \
		gain.o \
		rtpeak.o \
		conv.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@


MODS += soxr.so
LIBS3 += $(ALIB3_BIN)/libsoxr-phi.so
soxr.o: $(PHIOLA)/src/afilter/soxr.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/afilter/soxr*.h)
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
soxr.so: soxr.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lsoxr-phi -o $@


MODS += format.so
%.o: $(PHIOLA)/src/format/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/format/*.h)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
cue-read.o: $(PHIOLA)/src/list/cue-read.c $(DEPS)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
m3u.o: $(PHIOLA)/src/list/m3u.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/list/m3u-*.h)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
pls-read.o: $(PHIOLA)/src/list/pls-read.c $(DEPS)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
format.so: mod-fmt.o \
		aac-adts.o \
		ape-read.o \
		avi.o \
		caf.o \
		flac-fmt.o flac-ogg.o \
		mkv.o \
		mp3.o \
		mp4.o \
		mpc-read.o \
		ogg.o \
		wav.o \
		wv.o \
		\
		cue-read.o \
		m3u.o \
		pls-read.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

# CODECS LOSSY

MODS += aac.so
LIBS3 += $(ALIB3_BIN)/libfdk-aac-phi.so
aac.o: $(PHIOLA)/src/acodec/aac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/aac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/aac.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
aac.so: aac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lfdk-aac-phi -o $@

MODS += mpeg.so
LIBS3 += $(ALIB3_BIN)/libmpg123-phi.so
mpeg.o: $(PHIOLA)/src/acodec/mpeg.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/mpeg-*.h)
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
mpeg.so: mpeg.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmpg123-phi -o $@

# CODECS LOSSLESS

MODS +=  alac.so
LIBS3 += $(ALIB3_BIN)/libALAC-phi.so
alac.o: $(PHIOLA)/src/acodec/alac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/alac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/alac.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
alac.so: alac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lALAC-phi -o $@

MODS += flac.so
LIBS3 += $(ALIB3_BIN)/libFLAC-phi.so
flac.o: $(PHIOLA)/src/acodec/flac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/flac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/flac.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
flac.so: flac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lFLAC-phi -o $@


MODS += zstd.so
LIBS3 += $(FFPACK_BIN)/libzstd-ffpack.so
%.o: $(PHIOLA)/src/dfilter/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/dfilter/zstd-*.h)
	$(C) $(CFLAGS) -I$(FFPACK) $< -o $@
zstd.so: zstd.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(FFPACK_BIN) -lzstd-ffpack -o $@


libs: libphiola.so.debug \
		$(MODS:.so=.so.debug)
	cp -u $(LIBS3) *.so *.debug $(LIB_DIR)


OBJCOPY := $(C_DIR)/llvm-objcopy
STRIP := $(C_DIR)/llvm-strip
%.debug: %
	$(OBJCOPY) --only-keep-debug $< $@
	$(STRIP) $<
	$(OBJCOPY) --add-gnu-debuglink=$@ $<
	touch $@
