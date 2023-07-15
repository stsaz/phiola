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
	$(SUBMAKE) CPU=amd64 A_API=26 libs

lib-arm64:
	$(MKDIR) obj-arm64 $(LIB_DIR_ARM64)
	$(SUBMAKE) CPU=arm64 A_API=26 libs

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


$(OBJ_DIR)/%.o: $(PHIOLA)/src/core/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/core/*.h) \
		$(wildcard $(PHIOLA)/src/util/*.h)
	$(C) $(CFLAGS) $< -o $@
$(OBJ_DIR)/%.o: $(PHIOLA)/src/queue/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/queue/*.h)
	$(C) $(CFLAGS) $< -o $@
$(OBJ_DIR)/%.o: $(PHIOLA)/src/jni/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/jni/*.h)
	$(C) $(CFLAGS) $< -o $@
$(LIB_DIR)/libphiola.$(SO): \
		$(OBJ_DIR)/phiola-jni.o \
		$(OBJ_DIR)/core.o \
		$(OBJ_DIR)/auto.o \
		$(OBJ_DIR)/dir-read.o \
		$(OBJ_DIR)/file.o\
		$(OBJ_DIR)/qu.o \
		$(OBJ_DIR)/track.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_PTHREAD) -o $@


MODS += $(LIB_DIR)/aaudio.so.debug
$(OBJ_DIR)/%.o: $(PHIOLA)/src/adev/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/adev/*.h)
	$(C) $(CFLAGS) -I$(FFAUDIO) $< -o $@
$(OBJ_DIR)/ffaudio-aaudio.o: $(FFAUDIO)/ffaudio/aaudio.c
	$(C) -I$(FFAUDIO) $(CFLAGS) $< -o $@
$(LIB_DIR)/aaudio.$(SO): $(OBJ_DIR)/aaudio.o $(OBJ_DIR)/ffaudio-aaudio.o
	$(LINK) -shared $+ $(LINKFLAGS) -laaudio -o $@


MODS += $(LIB_DIR)/afilter.so.debug
$(OBJ_DIR)/%.o: $(PHIOLA)/src/afilter/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/afilter/*.h)
	$(C) $(CFLAGS) $< -o $@
$(OBJ_DIR)/crc.o: $(PHIOLA)/3pt/crc/crc.c
	$(C) $(CFLAGS) $< -o $@
$(LIB_DIR)/afilter.$(SO): $(OBJ_DIR)/afilter.o \
		$(OBJ_DIR)/crc.o \
		$(OBJ_DIR)/peaks.o \
		$(OBJ_DIR)/gain.o \
		$(OBJ_DIR)/rtpeak.o \
		$(OBJ_DIR)/conv.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@


MODS += $(LIB_DIR)/format.so.debug
$(OBJ_DIR)/%.o: $(PHIOLA)/src/format/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/format/*.h)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
$(OBJ_DIR)/m3u.o: $(PHIOLA)/src/list/m3u.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/list/m3u-*.h)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
$(OBJ_DIR)/pls-read.o: $(PHIOLA)/src/list/pls-read.c $(DEPS)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
$(LIB_DIR)/format.$(SO): $(OBJ_DIR)/mod-fmt.o \
		$(OBJ_DIR)/aac-adts.o \
		$(OBJ_DIR)/ape-read.o \
		$(OBJ_DIR)/avi.o \
		$(OBJ_DIR)/caf.o \
		$(OBJ_DIR)/flac-fmt.o $(OBJ_DIR)/flac-ogg.o \
		$(OBJ_DIR)/mkv.o \
		$(OBJ_DIR)/mp3.o \
		$(OBJ_DIR)/mp4.o \
		$(OBJ_DIR)/mpc-read.o \
		$(OBJ_DIR)/ogg.o \
		$(OBJ_DIR)/wav.o \
		$(OBJ_DIR)/wv.o \
		\
		$(OBJ_DIR)/m3u.o \
		$(OBJ_DIR)/pls-read.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

# CODECS LOSSY

MODS += $(LIB_DIR)/aac.$(SO).debug
LIBS3 += $(ALIB3_BIN)/libfdk-aac-phi.$(SO)
$(OBJ_DIR)/aac.o: $(PHIOLA)/src/acodec/aac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/aac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/aac.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
$(LIB_DIR)/aac.$(SO): $(OBJ_DIR)/aac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lfdk-aac-phi -o $@

MODS += $(LIB_DIR)/mpeg.$(SO).debug
LIBS3 += $(ALIB3_BIN)/libmpg123-phi.$(SO)
$(OBJ_DIR)/mpeg.o: $(PHIOLA)/src/acodec/mpeg.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/mpeg-*.h)
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
$(LIB_DIR)/mpeg.$(SO): $(OBJ_DIR)/mpeg.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmpg123-phi -o $@

# CODECS LOSSLESS

MODS +=  $(LIB_DIR)/alac.$(SO).debug
LIBS3 += $(ALIB3_BIN)/libALAC-phi.$(SO)
$(OBJ_DIR)/alac.o: $(PHIOLA)/src/acodec/alac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/alac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/alac.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
$(LIB_DIR)/alac.$(SO): $(OBJ_DIR)/alac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lALAC-phi -o $@

MODS += $(LIB_DIR)/flac.$(SO).debug
LIBS3 += $(ALIB3_BIN)/libFLAC-phi.$(SO)
$(OBJ_DIR)/flac.o: $(PHIOLA)/src/acodec/flac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/flac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/flac.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
$(LIB_DIR)/flac.$(SO): $(OBJ_DIR)/flac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lFLAC-phi -o $@


MODS += $(LIB_DIR)/zstd.$(SO)
LIBS3 += $(FFPACK_BIN)/libzstd-ffpack.$(SO)
$(OBJ_DIR)/%.o: $(PHIOLA)/src/dfilter/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/dfilter/zstd-*.h)
	$(C) $(CFLAGS) -I$(FFPACK) $< -o $@
$(LIB_DIR)/zstd.$(SO): $(OBJ_DIR)/zstd.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(FFPACK_BIN) -lzstd-ffpack -o $@


libs: $(LIB_DIR)/libphiola.so.debug \
	$(MODS)
	cp -u $(LIBS3) $(LIB_DIR)


OBJCOPY := $(C_DIR)/llvm-objcopy
STRIP := $(C_DIR)/llvm-strip
%.debug: %
	$(OBJCOPY) --only-keep-debug $< $@
	$(STRIP) $<
	$(OBJCOPY) --add-gnu-debuglink=$@ $<
	touch $@
