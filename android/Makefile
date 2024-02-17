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

LIB_DIR_CPU := x86_64
ifeq "$(CPU)" "arm64"
	LIB_DIR_CPU := arm64-v8a
else ifeq "$(CPU)" "arm"
	LIB_DIR_CPU := armeabi-v7a
endif
log-bt:
	$(SDK_DIR)/platform-tools/adb $(ADB_FLAGS) logcat -v time | $(NDK_DIR)/ndk-stack -sym phiola/libs/$(LIB_DIR_CPU)


# Start Android emulator

AVD := @Pixel_6_API_33
emulator:
	$(SDK_DIR)/emulator/emulator -list-avds
	$(SDK_DIR)/emulator/emulator $(AVD)


# Directories
ROOT_DIR := ../..
PHIOLA := $(ROOT_DIR)/phiola
FFBASE := $(ROOT_DIR)/ffbase
FFSYS := $(ROOT_DIR)/ffsys

include $(FFBASE)/conf.mk
include andk.mk
SYS := android

LIB_DIR_AMD64 := $(PHIOLA)/android/phiola/libs/x86_64
LIB_DIR_ARM64 := $(PHIOLA)/android/phiola/libs/arm64-v8a
LIB_DIR_ARM := $(PHIOLA)/android/phiola/libs/armeabi-v7a

lib-amd64:
	$(MKDIR) ../_android-amd64 $(LIB_DIR_AMD64)
	$(MAKE) libs \
		-C ../_android-amd64 \
		-f $(PHIOLA)/android/Makefile \
		-I $(PHIOLA)/android \
		CPU=amd64

lib-arm64:
	$(MKDIR) ../_android-arm64 $(LIB_DIR_ARM64)
	$(MAKE) libs \
		-C ../_android-arm64 \
		-f $(PHIOLA)/android/Makefile \
		-I $(PHIOLA)/android \
		CPU=arm64

clean-lib:
	$(RM) ../_android-arm64 ../_android-amd64 $(LIB_DIR_AMD64) $(LIB_DIR_ARM64)


LIB_DIR := $(LIB_DIR_AMD64)
ifeq "$(CPU)" "arm64"
	LIB_DIR := $(LIB_DIR_ARM64)
else ifeq "$(CPU)" "arm"
	LIB_DIR := $(LIB_DIR_ARM)
endif
CFLAGS := $(A_CFLAGS)
CFLAGS += -DFFBASE_MEM_ASSERT
CFLAGS += -MMD -MP
CFLAGS += -I$(FFBASE)
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-for-loop-analysis
CFLAGS += -g
CFLAGS += -fPIC
ifeq "$(DEBUG)" "1"
	CFLAGS += -DFF_DEBUG -O0 -Werror
else
	CFLAGS += -O3 -fno-strict-aliasing -fvisibility=hidden
endif
CFLAGS += $(CFLAGS_USER)
CFLAGS_BASE := $(CFLAGS)
CFLAGS += -I$(PHIOLA)/src -I$(FFSYS)
LINKFLAGS := $(A_LINKFLAGS) \
	-llog

%.o: $(FFBASE)/ffbase/%.c
	$(C) $(CFLAGS) -DFFBASE_OPT_SIZE $< -o $@

-include $(wildcard *.d)

include $(PHIOLA)/src/core/Makefile
include $(PHIOLA)/src/adev/Makefile
include $(PHIOLA)/src/afilter/Makefile
include $(PHIOLA)/src/format/Makefile
include $(PHIOLA)/src/acodec/Makefile
include $(PHIOLA)/src/dfilter/Makefile

libs: libphiola.so.debug \
		$(MODS:.so=.so.debug)
	cp -u $(LIBS3) *.so *.debug \
		$(LIB_DIR)
	bash ../android/build-rename-libs.sh $(LIB_DIR) $(MODS)


OBJCOPY := $(C_DIR)/llvm-objcopy
STRIP := $(C_DIR)/llvm-strip
%.debug: %
	$(OBJCOPY) --only-keep-debug $< $@
	$(STRIP) $<
	$(OBJCOPY) --add-gnu-debuglink=$@ $<
	touch $@
